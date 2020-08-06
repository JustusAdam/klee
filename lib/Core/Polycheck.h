#include "Memory.h"
#include "klee/Module/KInstruction.h"

#include "uniqtype-defs.h"

namespace klee {

class Polycheck {
public:
    typedef int64_t address;
    typedef struct uniqtype t;
    typedef unsigned klee_loc;

    Polycheck();
    Polycheck(Polycheck &&pc);

    void registerExternal(const MemoryObject &mo);
    void registerGlobal(const MemoryObject &mo);
    void registerVararg(const MemoryObject &mo);
    void registerAlloc(const MemoryObject &mo, const KInstruction& allocSite);
    void registerArgv(const MemoryObject &mo);
    void registerArgc(const MemoryObject &mo);

    void handleTypecheck(
        ExecutionState &state,
        Executor &executor,
        KInstruction* target,
        ref<Expr> arg1, ref<Expr> arg2
        );


private:
    typedef struct loc {
        const std::string & file;
        unsigned line_start;
        unsigned line_end;

        std::string as_string() const;
        bool operator<(const struct loc & rhs) const;
    } loc;
    typedef struct allocation_info { 
        const MemoryObject& mem_obj;
        const t* type; 
    } allocation_info;
    typedef std::map<loc, t> type_map;
    typedef std::map<address, allocation_info> alloc_map;

    static type_map * typeMap;

    static void init_type_map();
    static const t * resolveType(const loc& loc);

    alloc_map * allocMap;

    void recordAllocationInfo(allocation_info i);

    bool typecheck(const t* target, const t* examined);
    const t& resolveTypeFromLoc(const klee_loc& loc);
    const allocation_info& lookupAllocation(const MemoryObject& mo);
};
}