extern "C" {
#include <mono/jit/mono-private-unstable.h>
#include <mono/jit/jit.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/class.h>
#include <mono/metadata/threads.h>
}

#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <pthread.h>
#include <libgen.h>

#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <vector>
#include <algorithm>

#ifdef _WIN32
#define PATH_SEP_CHAR ';'
#else
#define PATH_SEP_CHAR ':'
#endif


namespace util
{
	template< typename... Args >
	std::string ssprintf(const char *format, Args... args) {
		int length = std::snprintf(nullptr, 0, format, args...);
		assert(length >= 0);

		std::vector<char> buf(length + 1);
		std::snprintf(buf.data(), length + 1, format, args...);

		std::string str(buf.data());
		return str;
	}

    template <typename T>
    std::basic_string<T> lowercase(const std::basic_string<T>& s)
    {
        std::basic_string<T> s2 = s;
        std::transform(s2.begin(), s2.end(), s2.begin(),
                       [](const T v){ return static_cast<T>(std::tolower(v)); });
        return s2;
    }
}

static void *
run_something (void *ptr);

/* This is a magic number that must be passed to mono_jit_init_version */
#define FRAMEWORK_VERSION "v4.0.30319"

static pthread_t main_thread;

struct run_params {
	MonoImage *img;
	const char *sample_assm;
};

template<typename T>
std::optional<T> json_get_opt(const nlohmann::json& j, const std::string& key) {
	if(j.contains(key)){
		return j.at(key);
	}
	return std::nullopt;
}

static nlohmann::json load_deps_json(const char *depsFile){
	std::ostringstream tpa;

	std::ifstream file(depsFile);
	if(!file.is_open()){
		throw std::runtime_error(util::ssprintf("Cannot open %s for reading", depsFile));
	}


	nlohmann::json deps{};
	try {
		file >> deps;
	} catch(const nlohmann::json::parse_error& e){
		printf("failed to parse %s\n", depsFile);
	}
	file.close();

    return deps;
}

struct ctx {
    std::string main_asm_name;
    std::string main_asm_fqdn;
    std::string tpa_list;
};

#define GET_BREAK(t, p, n) ({ \
        auto v = json_get_opt<t>(p, n); if(!v.has_value()) break; \
    	v.value(); })

#define GET_CONT(t, p, n) ({ \
        auto v = json_get_opt<t>(p, n); if(!v.has_value()) continue; \
    	v.value(); })

#define GETJ_B(p, n) GET_BREAK(nlohmann::json, p, n)
#define GETS_B(p, n) GET_BREAK(std::string, p, n)
#define GETJ_C(p, n) GET_CONT(nlohmann::json, p, n)
#define GETS_C(p, n) GET_CONT(std::string, p, n)

static struct ctx build_tpa_list(const char *depsFile, const nlohmann::json &deps){
    std::filesystem::path depsFs(depsFile);
    auto deps_dir = depsFs.parent_path();

	std::ostringstream tpa;
	bool tpa_first = true;
	auto tpa_append = [&](const std::string& itm){
		if(tpa_first){
			tpa_first = false;
		} else {
#ifdef _WIN32
			tpa << ';';
#else
			tpa << ':';
#endif
		}
		tpa << itm;
	};

    struct ctx result;
	do {
		// get the framework name and RID
		auto runtimeTarget = GETJ_B(deps, "runtimeTarget");
		auto runtimeTarget_name = GETS_B(runtimeTarget, "name");

		// get the targets exported by the framework
		auto all_targets = GETJ_B(deps, "targets");
		auto framework_target = GETJ_B(all_targets, runtimeTarget_name);

		std::optional<std::pair<std::string, nlohmann::json>> project_node = std::nullopt;

		// get all libraries contained in this bundle
		auto libraries = GETJ_B(deps, "libraries");

		// look for a "project" item
		for(const auto& itm : libraries.items()){
			auto lib_name = itm.key();
			auto lib_item = itm.value();
			auto lib_type = GETS_C(lib_item, "type");
			if(!project_node.has_value() && lib_type == "project"){
				project_node = {lib_name, lib_item};
				break;
			}
		}
        result.main_asm_name = project_node->first;

		if(!project_node.has_value()) break;

		// get the project node
		auto project_target = GETJ_B(framework_target, project_node->first);

        auto slash = project_node->first.find('/');
        if(slash == std::string::npos){
            // no version specifier
            break;
        }
        result.main_asm_fqdn = util::ssprintf("%s, Version=%s",
                                       project_node->first.substr(0, slash).c_str(),
                                       project_node->first.substr(slash + 1).c_str());


        auto project_rt = GETJ_B(project_target, "runtime");
        for(const auto& itm : project_rt.items()) {
            const auto& lib_name = itm.key();
            if(util::lowercase(lib_name).ends_with(".dll")) {
                tpa_append((deps_dir / lib_name).string());
            }
        }

		// process the project dependencies
		auto project_deps = GETJ_B(project_target, "dependencies");
		for(const auto& itm : project_deps.items()){
			const auto& item_name = itm.key();
			std::string item_version = itm.value();

			auto item_key = util::ssprintf("%s/%s",
										   item_name.c_str(),
										   item_version.c_str());

			auto target_item = GETJ_C(framework_target, item_key);
			auto target_runtime = json_get_opt<nlohmann::json>(target_item, "runtime");
			auto target_native = json_get_opt<nlohmann::json>(target_item, "native");

			if(target_runtime.has_value()){
				for(const auto& lib : target_runtime.value().items()){
					const auto& lib_name = lib.key();
					if(util::lowercase(lib_name).ends_with(".dll")) {
						tpa_append((deps_dir / lib_name).string());
					}
				}
			}
			if(target_native.has_value()){
                for(const auto& lib : target_native.value().items()) {
                    const auto& lib_name = lib.key();
                    if (util::lowercase(lib_name).ends_with(".dll")) {
                        tpa_append((deps_dir / lib_name).string());
                    }
                }
            }
		}
	} while(false);

    result.tpa_list = tpa.str();
    return result;
}

static MonoAssembly *load_asm_by_name(const char *asmName){
    MonoAssemblyName *aname = mono_assembly_name_new (asmName);
    if (!aname) {
        printf ("Couldn't parse assembly name '%s'\n", asmName);
        return NULL;
    }
    MonoImageOpenStatus status;
    MonoAssembly *assembly = mono_assembly_load_full (aname, /*basedir*/ NULL, &status, 0);
    if (!assembly || status != MONO_IMAGE_OK) {
        printf ("Couldn't open \"%s\", (status=0x%08x)\n", asmName, status);
        mono_assembly_name_free (aname);
        return NULL;
    }
    mono_assembly_name_free (aname);
    return assembly;
}

static MonoAssembly *load_asm_by_path(const char *asmPath){
    MonoImageOpenStatus status;
    MonoAssembly *assembly = mono_assembly_open_full(asmPath, &status, false);
    if (!assembly || status != MONO_IMAGE_OK) {
        printf ("Couldn't open \"%s\", (status=0x%08x)\n", asmPath, status);
        return NULL;
    }
    return assembly;
}

int
main (int argc, char *argv[])
{
	if(argc < 2){
		fprintf(stderr, "Usage: %s [depsJson]\n", argv[0]);
		return EXIT_FAILURE;
	}

    const char *depsFile = argv[1];
    nlohmann::json deps = load_deps_json(depsFile);
    auto tpa_list = build_tpa_list(depsFile, deps);

    const char *prop_keys[] = {"TRUSTED_PLATFORM_ASSEMBLIES"};
    const char *prop_values[] = {tpa_list.tpa_list.c_str()};
    int nprops = sizeof(prop_keys)/sizeof(prop_keys[0]);

    monovm_initialize (nprops, (const char**) &prop_keys, (const char**) &prop_values);
    MonoDomain *root_domain = mono_jit_init_version ("embedder_sample", FRAMEWORK_VERSION);

    if (!root_domain) {
        printf ("root domain was null, expected non-NULL on success\n");
        return EXIT_FAILURE;
    }
    printf ("runtime initialized\n");
    printf("loading %s\n", tpa_list.main_asm_fqdn.c_str());

    MonoAssembly *assembly = load_asm_by_name(tpa_list.main_asm_fqdn.c_str());
    MonoImage *img = mono_assembly_get_image (assembly);

    bool use_threads = true;
    void *result = (void *)EXIT_FAILURE;

    struct run_params params = {
            .img = img,
            .sample_assm = tpa_list.main_asm_name.c_str()
    };

    if (use_threads) {
        printf ("== running on a foreign thread\n");

        main_thread = pthread_self ();

        pthread_attr_t attr;
        pthread_attr_init (&attr);

        pthread_t thr;

        if (pthread_create (&thr, &attr, &run_something, &params) != 0)
        {
            perror ("could not create thread");
            return 1;
        }
        if (pthread_join (thr, &result) != 0) {
            perror ("could not join thread");
            return 1;
        }
    }

    if (!result) {
        printf ("== running on the main thread\n");
        result = run_something (&params);
    }
    return (int)(intptr_t)result;
}

static void *
run_something (void * start_data)
{
	auto params = (struct run_params*)start_data;

	MonoThread *thread = NULL;
	if (!pthread_equal (pthread_self(), main_thread)) {
		thread = mono_thread_attach (mono_get_root_domain ());
 		printf ("%% attached foreign thread\n");
	}

	MonoClass *kls = mono_class_from_name (params->img, "CSharpSample", "SampleClass");
	if (!kls) {
		printf ("Coudln't find CSharpSample.SampleClass in \"%s\"\n", params->sample_assm);
		return (void*)(intptr_t)1;
	}

	MonoMethod *create = mono_class_get_method_from_name (kls, "Create", 0);
	if (!create) {
		printf ("No Create method in CSharpSample.SampleClass\n");
		return (void*)(intptr_t)1;
	}

	void *args[1];
	MonoObject *exc;

	MonoObject *obj = mono_runtime_invoke (create, NULL, (void**)&args, NULL);

	MonoMethod *hello = mono_class_get_method_from_name (kls, "Hello", 0);

	if (!hello) {
		printf ("No Hello method in CSharpSample.SampleClass\n");
		return (void*)(intptr_t)1;
	}

	mono_runtime_invoke (hello, obj, (void**)&args, NULL);

	if (thread) {
		mono_thread_detach (thread);
		thread = NULL;
		printf ("%% detached\n");
	}


	if (!pthread_equal (pthread_self(), main_thread)) {
		thread = mono_thread_attach (mono_get_root_domain ());
 		printf ("%% attached again\n");
	}

	obj = mono_runtime_invoke (create, NULL, (void**)&args, NULL);

	mono_runtime_invoke (hello, obj, (void**)&args, NULL);

	if (thread) {
		mono_thread_detach (thread);
		thread = NULL;
		printf ("%% detached again\n");
	}

	fflush (stdout);

	return (void *)(intptr_t)0;
}
