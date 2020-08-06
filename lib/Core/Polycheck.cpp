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

#include "liballocs.h"

llvm::cl::opt<std::string> MetaBasePath(
    "polycheck-meta-base",
    llvm::cl::desc("Basepath for the metadata used by polycheck")
);

using namespace klee;

Polycheck::Polycheck() : allocMap(new Polycheck::alloc_map()) {}

Polycheck::Polycheck(Polycheck &&pc) = default;

void Polycheck::init_type_map() {
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
        getline(f, buf, '\t');

        printf("ignoring %s\n", buf.c_str());
        buf.clear();
        
        getline(f, buf, '\t');

        auto type = (t*) dlsym(dlhandle, buf.c_str());


        printf("file? %s line %i-%i typename '%s'\n", file->c_str(), line_start, line_end, buf.c_str());

        assert(type);

        f.ignore('\n');

        typeMap->emplace(
            loc {
                .file = *file,
                .line_start = line_start,
                .line_end = line_end
            },
            *type
        );
    }
    printf("Type map loaded!\n");
}

bool Polycheck::loc::operator<(const struct loc& rhs) const {
    return file < rhs.file && line_start < rhs.line_start && line_end < rhs.line_end;
}

std::string Polycheck::loc::as_string() const {
    return file + ":" + std::to_string(line_start) + "-" + std::to_string(line_end);
}

Polycheck::type_map * Polycheck::typeMap = nullptr;

const Polycheck::t * Polycheck::resolveType(const Polycheck::loc& loc) {
    if (typeMap == nullptr)
        init_type_map();

    auto result = typeMap->find(loc);

    if (result == typeMap->end())
        klee_error("unable to resolve type for location %s", loc.as_string().c_str());
    else {
        return &result->second;
    }
}

void Polycheck::registerExternal(const MemoryObject &mo) {}
void Polycheck::registerGlobal(const MemoryObject &mo) {}
void Polycheck::registerVararg(const MemoryObject &mo) {}
void Polycheck::registerArgv(const MemoryObject &mo) {}
void Polycheck::registerArgc(const MemoryObject &mo) {}

void Polycheck::registerAlloc(const MemoryObject& mo, const KInstruction& allocSite) {
    // This is proabably wrong
    auto loc = Polycheck::loc {
        .file = allocSite.info->file,
        .line_start = allocSite.info->line,
        .line_end = allocSite.info->line,
    };
    const t* type = resolveType(loc);

    recordAllocationInfo(allocation_info { 
        .mem_obj = mo,
        .type = type
    });
}

void Polycheck::recordAllocationInfo(const Polycheck::allocation_info i) {
    assert(allocMap);
    allocMap->emplace(i.mem_obj.address, i);
}

void Polycheck::handleTypecheck(
    ExecutionState &state,
    Executor &executor,
    KInstruction* target,
    ref<Expr> arg1,
    ref<Expr> arg2) {
  assert(isa<ConstantExpr>(*arg1));
  auto e1 = cast<ConstantExpr>(arg1);
/*  llvm::CallSite cs(state.prevPC->inst);
   const Polycheck::klee_loc loc = cs.getArgument(0).;
  const Polycheck::t &argtype = resolveTypeFromLoc(loc); */
  ObjectPair result;
  assert(state.addressSpace.resolveOne(e1, result));
  const t* argtype = lookupAllocation(*result.first).type;
  Executor::ExactResolutionList resolutions;
  executor.resolveExact(state, arg2, resolutions, "unnamed");
  for (auto &it : resolutions) {
    // This also gives me a new ExecutionState, do I need to do something with that?
    executor.polycheck->typecheck(argtype, 
     lookupAllocation(*it.first.first).type // this is probably wrong, I think I may have to get the offset from the object state as well
     );
  }
}

bool Polycheck::typecheck(const t* target, const t * examined) {
    unsigned target_offset_witin_uniqtype;
    t* cur_obj_uniqtype = const_cast<t*>(examined);
    t* test_uniqtype = const_cast<t*>(target);
    t *last_attempted_uniqtype;
    unsigned last_uniqtype_offset;
    unsigned cumulative_offset_searched;
    t* cur_containing_uniqtype;
    uniqtype_rel_info * cur_contained_pos;
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

const Polycheck::allocation_info& Polycheck::lookupAllocation(const MemoryObject& mo) {
    auto res = allocMap->find(mo.address);

    if (res == allocMap->end())
        klee_error("Looking up an allocation failed at address %lu", mo.address);
    else
        return res->second;
}