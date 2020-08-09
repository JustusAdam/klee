#include "Polycheck.h"
#include "Memory.h"
#include "ExecutionState.h"
#include "Executor.h"

#include "klee/Support/ErrorHandling.h"

#include "llvm/IR/CallSite.h"
#include "llvm/Support/CommandLine.h"
#include <fstream>
#include <string.h>
#include <dlfcn.h>
#include <unistd.h>

#include "liballocs.h"

llvm::cl::opt<std::string> MetaBasePath(
    "polycheck-meta-base",
    llvm::cl::desc("Basepath for the metadata used by polycheck")
);

using namespace klee;

Polycheck::Polycheck() : allocMap(new Polycheck::alloc_map()) {}

Polycheck::Polycheck(Polycheck &&pc) = default;

void Polycheck::init_type_map() 
{
    if (typeMap != nullptr) return;

    typeMap = new type_map();

    std::ifstream f((MetaBasePath + ".allocs").c_str());

    std::string buf;
    buf.reserve(200);

    void * dlhandle = dlopen((MetaBasePath + "-meta.so").c_str(), RTLD_LAZY);

    assert(dlhandle);

    while (!f.eof()) {

        for (unsigned i=0; i<3;i++) {
            getline(f, buf, '\t');
            buf.clear();
        }

        auto file = new std::string();
        getline(f, *file, '\t');

        assert(!file->empty());

        unsigned line_start;
        f >> line_start;

        unsigned line_end;
        f >> line_end;

        assert(f.get() == '\t');

        auto alloc_type = new std::string();
        getline(f, *alloc_type, '\t');

        auto type_name = new std::string();
        getline(f, *type_name, '\t');

        auto type = (t*) dlsym(dlhandle, type_name->c_str());


        printf("file? %s line %i-%i typename '%s'\n", 
            file->c_str(), line_start, line_end, type_name->c_str());

        assert(type);

        f.ignore('\n');

        loc the_loc = 
            loc {
                .file = *file,
                .line_start = line_start,
                .line_end = line_end
            };
        for (unsigned i = line_start; i <= line_end; i++) {
            auto it = typeMap->find(the_loc.as_index(i));
            if (it == typeMap->end()) {
                auto v = new type_map_record();
                v->push_back(
                    alloc_site_info_t {
                        .type = type,
                        .type_name = *type_name,
                        .alloc_type = *alloc_type,
                        .alloc_loc = the_loc
                    }
                );
                typeMap->emplace(the_loc.as_index(i), v);
            }
        }
    }
    printf("Type map loaded!\n");
    dump_type_map();
}

const Polycheck::index Polycheck::loc::as_index(unsigned i) const {
    return file + ":" + std::to_string(i);
}
const Polycheck::index Polycheck::loc::as_index() const {
    return as_index(line_start);
}

std::string Polycheck::loc::as_string() const 
{
    return file + ":" + std::to_string(line_start) + "-" + std::to_string(line_end);
}

Polycheck::type_map * Polycheck::typeMap = nullptr;

void Polycheck::dump_type_map()
{
    for (auto it : *typeMap)
    {
        printf("%s\n", it.first.c_str());
    }
}


const Polycheck::type_map_record * Polycheck::resolveInfo(
    const Polycheck::loc& loc) 
{
    if (typeMap == nullptr)
        init_type_map();
    printf("Searching for %s in\n", loc.as_index().c_str());
    auto result = typeMap->find(loc.as_index());

    if (result == typeMap->end()) {
        return nullptr;
    } else {
        return result->second;
    }
}

void Polycheck::registerExternal(const MemoryObject &mo) {}
void Polycheck::registerGlobal(const MemoryObject &mo) {}
void Polycheck::registerVararg(const MemoryObject &mo) {}
void Polycheck::registerArgv(const MemoryObject &mo) {}
void Polycheck::registerArgc(const MemoryObject &mo) {}

char* Polycheck::__wd = nullptr;

const std::string Polycheck::get_wd() {
    if (__wd == nullptr) {
        __wd = new char[PATH_MAX];
        getcwd(__wd, PATH_MAX);
    }

    return std::string(__wd);
}

void Polycheck::registerAlloc(
    const MemoryObject& mo, 
    const KInstruction& allocSite) 
{
    // This is proabably wrong
    auto loc = Polycheck::loc {
        .file = get_wd() + "/" + allocSite.info->file,
        .line_start = allocSite.info->line,
        .line_end = allocSite.info->line,
    };
    printf("Searching for allocation at %s:%u-%u\n", loc.file.c_str(), loc.line_start, loc.line_end);
    auto infos = resolveInfo(loc);

    if (!infos) {
        klee_warning("Could not find allocation info for %s", loc.as_index().c_str());
        return;
    }

    for (auto info : *infos)
        printf("Found allocation at %s:%u-%u\n", info.alloc_loc.file.c_str(), info.alloc_loc.line_start, info.alloc_loc.line_end);

    assert(allocMap != nullptr);

    recordAllocationInfo(
        mo,
        infos
    );
}

void Polycheck::recordAllocationInfo(
    const MemoryObject& mo, 
    const type_map_record * info) 
{
    assert(allocMap != nullptr);
    printf("Size of map: %lu\n", allocMap->size());
    allocMap->emplace(mo.address, new allocation_info {
        .mem_obj = mo,
        .site_info = info
    });
}

void Polycheck::handleTypecheck(
    ExecutionState &state,
    Executor &executor,
    KInstruction* target,
    ref<Expr> arg1,
    ref<Expr> arg2) 
{
    assert(isa<ConstantExpr>(*arg1));
    auto e1 = cast<ConstantExpr>(arg1);
    /*  llvm::CallSite cs(state.prevPC->inst);
    const Polycheck::klee_loc loc = cs.getArgument(0).;
    const Polycheck::t &argtype = resolveTypeFromLoc(loc); */
    ObjectPair result;
    assert(state.addressSpace.resolveOne(e1, result));
    auto argtype = lookupAllocation(*result.first);
    assert(argtype.site_info->size() == 1);
    const alloc_site_info_t & arg_type_site = (*argtype.site_info)[0];
    printf("Found target type (%s) allocated at %s:%u\n", arg_type_site.type_name.c_str(), arg_type_site.alloc_loc.file.c_str(), arg_type_site.alloc_loc.line_start);
    Executor::ExactResolutionList resolutions;
    executor.resolveExact(state, arg2, resolutions, "unnamed");
    for (auto &it : resolutions) {
        // This also gives me a new ExecutionState, do I need to do something with that?
        auto allocs = lookupAllocation(*it.first.first);
        bool succeded = true;
        for (auto alloc : *allocs.site_info) {
            succeded = succeded && typecheck(
                arg_type_site.type,
                alloc.type // this is probably wrong, I think I may have to get the offset from the object state as well
                );
        }

        if (!succeded) {
            printf("All candidate allocations failed containment test.\n");
            for (auto &alloc : *allocs.site_info)
                printf("%s (%s)\n", 
                    alloc.type_name.c_str(),
                    alloc.alloc_type.c_str());
        }
    }
}

bool Polycheck::typecheck(const t* target, const t * examined) 
{
    unsigned target_offset_witin_uniqtype = 0;
    t* cur_obj_uniqtype = const_cast<t*>(examined);
    t* test_uniqtype = const_cast<t*>(target);
    t *last_attempted_uniqtype = nullptr;
    unsigned last_uniqtype_offset = 0;
    unsigned cumulative_offset_searched = 0;
    t* cur_containing_uniqtype = nullptr;
    uniqtype_rel_info * cur_contained_pos  = nullptr;
    return __liballocs_find_matching_subobject(
        target_offset_witin_uniqtype, 
        cur_obj_uniqtype, 
        test_uniqtype, 
        &last_attempted_uniqtype, 
        &last_uniqtype_offset, 
        &cumulative_offset_searched, 
        &cur_containing_uniqtype, 
        &cur_contained_pos);
}

const Polycheck::allocation_info& Polycheck::lookupAllocation(const MemoryObject& mo) 
{
    auto res = allocMap->find(mo.address);

    if (res == allocMap->end()) {
        klee_error("Looking up an allocation failed at address %lu", mo.address);
    } else
        return *res->second;
}