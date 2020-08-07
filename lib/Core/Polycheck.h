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
    typedef struct alloc_site_info {
        const t* type;
        const std::string& type_name;
        const std::string& alloc_type;
    } alloc_site_info_t;
    typedef struct allocation_info { 
        const MemoryObject& mem_obj;
        const alloc_site_info_t & site_info;
    } allocation_info;
    typedef std::map<loc, alloc_site_info_t> type_map;
    typedef std::map<address, const allocation_info> alloc_map;

    static type_map * typeMap;

    static void init_type_map();
    static const alloc_site_info_t & resolveInfo(const loc& loc);

    alloc_map * allocMap;

    void recordAllocationInfo(const MemoryObject& mo, const alloc_site_info_t & info);

    bool typecheck(const t* target, const t* examined);
    const t& resolveTypeFromLoc(const klee_loc& loc);
    const allocation_info& lookupAllocation(const MemoryObject& mo);
};
}