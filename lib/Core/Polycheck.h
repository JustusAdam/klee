#include <memory>
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
    typedef std::string index;
    typedef struct loc {
        const std::string & file;
        unsigned line_start;
        unsigned line_end;

        std::string as_string() const;

        const index as_index(unsigned i) const;
        const index as_index() const;
        bool is_more_specific(const loc & other) const;
    } loc;
    typedef struct alloc_site_info {
        const t* type;
        const std::string& type_name;
        const std::string& alloc_type;
        const loc alloc_loc;
    } alloc_site_info_t;
    typedef std::shared_ptr<alloc_site_info_t> type_map_record;
    typedef struct allocation_info { 
        const MemoryObject& mem_obj;
        const type_map_record site_info;
    } allocation_info;
    typedef std::unordered_map<index, std::unique_ptr<type_map_record>> type_map;
    typedef std::unordered_map<address, const allocation_info*> alloc_map;

    class Oracle {
        enum Offset { NONE, ADD };

        Offset offset;
        int successes;
        static int THRESHOLD;
    public:
        Oracle ();
        unsigned addOffset(unsigned line);
        bool maybeRetryOffset();
        template<typename T>
        void reportResult(T* ptr);
    };

    static type_map * typeMap;

    static void init_type_map();
    static const type_map_record resolveInfo(const loc& loc);
    static void dump_type_map();
    static char* __wd;
    static const std::string get_wd();

    alloc_map * allocMap;

    Oracle oracle;

    void recordAllocationInfo(
        const MemoryObject& mo, 
        const type_map_record info);

    static bool typecheck(const t* target, const t* examined);
    const t& resolveTypeFromLoc(const klee_loc& loc);
    const allocation_info* lookupAllocation(const MemoryObject& mo);
};
}