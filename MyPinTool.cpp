/*! @file
 *  The file implements a simple memory profiler tool using Intel PIN.
 *  The sample code is an example how Intel PIN capabilities
 *  can be used in memory profiling area.
 */

#include <iostream>
#include <cstdlib>
#include <unordered_map>

#include "pin.H"


/* ===================================================================== */
/* Common definitions */
/* ===================================================================== */

#define TLS_MARKER_TRUE \
    (reinterpret_cast<VOID *>(0x01))
#define TLS_MARKER_FALSE \
    (reinterpret_cast<VOID *>(0x00))

/* ===================================================================== */
/* Structures defined */
/* ===================================================================== */

typedef struct __MemInfoItem {
    UINT64    size;
    VOID     *retAddr;
} MemInfoItem;

/* ===================================================================== */
/* Classes defined */
/* ===================================================================== */

class Statistics {

public:
    static Statistics &GetInstance() {
        static Statistics instance;
        return instance;
    }

    UINT32 GetMallocCount() const {
        UINT32 retval = 0;
        PIN_MutexLock(&mallocGuard);
        retval = mallocCount;
        PIN_MutexUnlock(&mallocGuard);
        return retval;
    }

    UINT32 GetCallocCount() const {
        UINT32 retval = 0;
        PIN_MutexLock(&callocGuard);
        retval = callocCount;
        PIN_MutexUnlock(&callocGuard);
        return retval;
    }

    UINT32 GetReallocCount() const {
        UINT32 retval = 0;
        PIN_MutexLock(&reallocGuard);
        retval = reallocCount;
        PIN_MutexUnlock(&reallocGuard);
        return retval;
    }

    UINT64 GetMallocedBytes() const {
        UINT64 retval = 0;
        PIN_MutexLock(&mallocGuard);
        retval = mallocedBytes;
        PIN_MutexUnlock(&mallocGuard);
        return retval;
    }

    UINT64 GetCallocedBytes() const {
        UINT64 retval = 0;
        PIN_MutexLock(&callocGuard);
        retval = callocedBytes;
        PIN_MutexUnlock(&callocGuard);
        return retval;
    }

    UINT64 GetReallocedBytes() const {
        UINT64 retval = 0;
        PIN_MutexLock(&reallocGuard);
        retval = reallocedBytes;
        PIN_MutexUnlock(&reallocGuard);
        return retval;
    }

    std::unordered_map<VOID *, MemInfoItem> GetMemoryMapClone() const {
        PIN_MutexLock(&mapGuard);
        std::unordered_map<VOID *, MemInfoItem> retval = memMap;
        PIN_MutexUnlock(&mapGuard);
        return retval;
    }

    VOID CountMalloc( VOID *memAddr, UINT64 memSize, VOID *retAddr ) {
        PIN_MutexLock(&mallocGuard);
        mallocedBytes += memSize;
        ++mallocCount;
        PIN_MutexUnlock(&mallocGuard);

        PIN_MutexLock(&mapGuard);
        memMap[memAddr] = { memSize, retAddr };
        PIN_MutexUnlock(&mapGuard);
    }

    VOID CountCalloc( VOID *memAddr, UINT64 memSize, VOID *retAddr ) {
        PIN_MutexLock(&callocGuard);
        callocedBytes += memSize;
        ++callocCount;
        PIN_MutexUnlock(&callocGuard);

        PIN_MutexLock(&mapGuard);
        memMap[memAddr] = { memSize, retAddr };
        PIN_MutexUnlock(&mapGuard);
    }

    VOID CountRealloc( VOID *memAddr, UINT64 memSize, VOID *retAddr  ) {
        PIN_MutexLock(&reallocGuard);
        reallocedBytes += memSize;
        ++reallocCount;
        PIN_MutexUnlock(&reallocGuard);
 
        if (memSize != 0) {
            PIN_MutexLock(&mapGuard);
            memMap[memAddr] = { memSize, retAddr };
            PIN_MutexUnlock(&mapGuard);
        }
    }

    VOID CountFree( VOID *memAddr ) {
        PIN_MutexLock(&mapGuard);
        memMap.erase(memAddr);
        PIN_MutexUnlock(&mapGuard);
    }

private:
    Statistics() :
        mallocCount(0U), callocCount(0U), reallocCount(0U),
        mallocedBytes(0LU), callocedBytes(0LU), reallocedBytes(0LU)
    {
        PIN_MutexInit(&mallocGuard);
        PIN_MutexInit(&callocGuard);
        PIN_MutexInit(&reallocGuard);
        PIN_MutexInit(&mapGuard);
    }

    ~Statistics() {
        PIN_MutexFini(&mallocGuard);
        PIN_MutexFini(&callocGuard);
        PIN_MutexFini(&reallocGuard);
        PIN_MutexFini(&mapGuard);
    }

    mutable PIN_MUTEX mallocGuard, callocGuard, reallocGuard;
    UINT32 mallocCount,   callocCount,   reallocCount;
    UINT64 mallocedBytes, callocedBytes, reallocedBytes;

    mutable PIN_MUTEX mapGuard;
    std::unordered_map<VOID *, MemInfoItem> memMap;
};

/* ===================================================================== */
/* Types defined */
/* ===================================================================== */

typedef INT32 (*MainPtr)     (INT32 argc, CHAR *argv[]);
typedef VOID  (*FiniPtr)     (VOID);
typedef INT32 (*BacktracePtr)(VOID**, INT32);

/* ===================================================================== */
/* Basic constants */
/* ===================================================================== */

const std::string __LIBC_START_MAIN = "__libc_start_main";
const std::string MALLOC            = "malloc";
const std::string CALLOC            = "calloc";
const std::string REALLOC           = "realloc";
const std::string FREE              = "free";
const std::string BACKTRACE         = "backtrace";

/* ===================================================================== */
/* Global Variables */
/* ===================================================================== */

Statistics   &g_stats             = Statistics::GetInstance();

BOOL          g_areWeInMain       = FALSE;
MainPtr       g_main              = nullptr;
FiniPtr       g_fini              = nullptr;

PIN_TLS_INDEX g_backtraceGuard    = OS_TlsAlloc(nullptr);
BacktracePtr  g_backtrace         = nullptr;

/* ===================================================================== */
/* Print Help Message                                                    */
/* ===================================================================== */

__attribute__((noreturn))
VOID Help( VOID ) {
    std::cout << "Memory Profiler PIN Tool." << std::endl;
    exit(EXIT_FAILURE);
}

/* ===================================================================== */

VOID *GetStackFrame() {
    const INT32 FRAMES_NUM_REQUIRED = 4;

    VOID *buffer[FRAMES_NUM_REQUIRED];
    INT32 nptrs = g_backtrace(buffer, FRAMES_NUM_REQUIRED);

    VOID *retAddr = nullptr;
    if (nptrs >= FRAMES_NUM_REQUIRED) {
        retAddr = buffer[FRAMES_NUM_REQUIRED - 1];
    }
    return retAddr;
}

/* ===================================================================== */

#define TRACE_ENABLED \
    (g_areWeInMain) && \
    (g_backtrace != nullptr) && \
    (OS_TlsGetValue(g_backtraceGuard) == TLS_MARKER_FALSE)

/* ===================================================================== */

VOID *WrapMalloc( VOID *(*realMalloc)(UINT64), UINT64 arg0 )
{
    VOID *memAddr = realMalloc( arg0 );

    UINT64 memSize = arg0;
    if (TRACE_ENABLED) {
        (void)OS_TlsSetValue(g_backtraceGuard, TLS_MARKER_TRUE); // prevents recursion in backtrace
        g_stats.CountMalloc(memAddr, memSize, GetStackFrame());
        (void)OS_TlsSetValue(g_backtraceGuard, TLS_MARKER_FALSE);
    }

    return memAddr;
}

/* ===================================================================== */

VOID *WrapCalloc( VOID *(*realCalloc)(UINT64, UINT64), UINT64 arg0, UINT64 arg1 )
{
    VOID *memAddr = realCalloc( arg0, arg1 );

    UINT64 memSize = arg0 * arg1;
    if (TRACE_ENABLED) {
        g_stats.CountCalloc(memAddr, memSize, GetStackFrame());
    }

    return memAddr;
}

/* ===================================================================== */

VOID *WrapRealloc( VOID *(*realRealloc)(VOID *, UINT64), VOID *arg0, UINT64 arg1 )
{
    VOID *memAddr = realRealloc( arg0, arg1 );

    UINT64 memSize = arg1;
    if (TRACE_ENABLED) {
        g_stats.CountRealloc(memAddr, memSize, GetStackFrame());
    }

    return memAddr;
}

/* ===================================================================== */

VOID WrapFree( VOID (*realFree)(VOID *), VOID *arg0 )
{
    realFree( arg0 );
    g_stats.CountFree(arg0);
}

/* ===================================================================== */

VOID PrintBytes( VOID *addr, UINT64 size ) {
    CHAR *bytes = static_cast<CHAR *>(addr);
    std::cout << std::setfill('0') << std::hex;
    for (UINT64 i = 0; i < size; i++) {
        std::cout << std::setw(2) << (static_cast<INT32>(bytes[i]) & 0xFF) << " ";
    }
    std::cout << std::setfill(' ') << std::dec;
}

/* ===================================================================== */

INT32 SetUp( INT32 argc, CHAR *argv[] ) {
    (void)OS_TlsSetValue(g_backtraceGuard, TLS_MARKER_FALSE);

    return g_main(argc, argv);
}

/* ===================================================================== */

VOID TearDown( VOID ) {
    g_fini();

    (void)OS_TlsFree(g_backtraceGuard);

    std::cout << std::endl;;
    std::cout << "SUMMARY START" << std::endl;

    std::unordered_map<VOID *, MemInfoItem> memoryMap = g_stats.GetMemoryMapClone();
    if (memoryMap.size() != 0) {
        UINT32 leaksCount = 0U;
        UINT64 bytesLeaked = 0LU;

        std::cout << std::endl << "Leaks dumped:" << std::endl;

        for (std::pair<VOID * const, MemInfoItem> memPair : memoryMap) {
            ++leaksCount;
            bytesLeaked += memPair.second.size;

            std::cout << std::setw(5) << leaksCount << ".) Address  : "
                << memPair.first << std::endl;
            std::cout << std::setw(8) << " " << "Hex dump : ";
            PrintBytes(memPair.first, memPair.second.size);
            std::cout << std::endl << std::setw(8) << " " << "Bytes    : "
                << memPair.second.size  << std::endl;
            std::cout<< std::setw(8) << " " << "Backtrace: "
                << memPair.second.retAddr << std::endl;
        }
        std::cout << std :: endl << "Leaks count " << leaksCount
            << ", bytes leaked " << bytesLeaked << std::endl;
    }
    else {
       std::cout << std::endl << "NO LEAKS FOUND!" << std::endl;
    }

    std::cout << std::endl<< "In detail:" << std::endl;
    std::cout << std::setw(4) << " " << "malloc's  count: "
        << std::setw(6) << g_stats.GetMallocCount() << std::endl;
    std::cout << std::setw(4) << " " << "calloc's  count: "
        << std::setw(6) << g_stats.GetCallocCount() << std::endl;
    std::cout << std::setw(4) << " " << "realloc's count: "
        << std::setw(6) << g_stats.GetReallocCount() << std::endl;
    std::cout << std::setw(4) << " " << "malloced  bytes: "
        << std::setw(6) << g_stats.GetMallocedBytes() << std::endl;
    std::cout << std::setw(4) << " " << "calloced  bytes: "
        << std::setw(6) << g_stats.GetCallocedBytes() << std::endl;
    std::cout << std::setw(4) << " " << "realloced bytes: "
        << std::setw(6) << g_stats.GetReallocedBytes() << std::endl;

    std::cout << std::endl<< "SUMMARY END" << std::endl;
}

/* ===================================================================== */

INT32 WrapLibcStartMain(
    INT32 (*realLibcStartMain)(VOID *, INT32, CHAR **, INT32, VOID *, VOID *, VOID *),
    VOID *arg0, INT32 arg1, CHAR **arg2, INT32 arg3, VOID *arg4, VOID *arg5, VOID *arg6 )
{
    g_main = reinterpret_cast<MainPtr>(arg0);
    g_fini = reinterpret_cast<FiniPtr>(arg5);

    g_areWeInMain = TRUE;

    return realLibcStartMain(
        reinterpret_cast<VOID *>(SetUp),
        arg1,
        arg2,
        arg3,
        arg4,
        reinterpret_cast<VOID *>(TearDown),
        arg6
    );
}

/* ===================================================================== */

VOID ImageLoad( IMG img, VOID *v )
{
    RTN rtn = RTN_FindByName( img, __LIBC_START_MAIN.c_str() );
    if (
        RTN_Valid(rtn) &&
        RTN_IsSafeForProbedInsertion(rtn)
    )
    {
        PROTO proto_libc_start_main = PROTO_Allocate( PIN_PARG(INT32), CALLINGSTD_DEFAULT,
            __LIBC_START_MAIN.c_str(), PIN_PARG(VOID *), PIN_PARG(INT32), PIN_PARG(CHAR **),
            PIN_PARG(INT32), PIN_PARG(VOID *), PIN_PARG(VOID *), PIN_PARG(VOID *), PIN_PARG_END() );
        RTN_ReplaceSignatureProbed(rtn, AFUNPTR(WrapLibcStartMain),
            IARG_PROTOTYPE, proto_libc_start_main,
            IARG_ORIG_FUNCPTR,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 4,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 5,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 6,
            IARG_END);
        PROTO_Free( proto_libc_start_main );
    }

    rtn = RTN_FindByName( img, MALLOC.c_str() );
    if (
        RTN_Valid(rtn) &&
        RTN_IsSafeForProbedInsertion(rtn)
    )
    {
        PROTO proto_malloc = PROTO_Allocate( PIN_PARG(VOID *), CALLINGSTD_DEFAULT,
            MALLOC.c_str(), PIN_PARG(UINT64), PIN_PARG_END() );
        RTN_ReplaceSignatureProbed(rtn, AFUNPTR(WrapMalloc),
            IARG_PROTOTYPE, proto_malloc,
            IARG_ORIG_FUNCPTR,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
            IARG_END);
        PROTO_Free( proto_malloc );
    }

    rtn = RTN_FindByName( img, CALLOC.c_str() );
    if (
        RTN_Valid(rtn) &&
        RTN_IsSafeForProbedInsertion(rtn)
    )
    {
        PROTO proto_calloc = PROTO_Allocate( PIN_PARG(VOID *), CALLINGSTD_DEFAULT,
            CALLOC.c_str(), PIN_PARG(UINT64), PIN_PARG(UINT64), PIN_PARG_END() );
        RTN_ReplaceSignatureProbed(rtn, AFUNPTR(WrapCalloc),
            IARG_PROTOTYPE, proto_calloc,
            IARG_ORIG_FUNCPTR,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
            IARG_END);
        PROTO_Free( proto_calloc );
    }

    rtn = RTN_FindByName( img, REALLOC.c_str() );
    if (
        RTN_Valid(rtn) &&
        RTN_IsSafeForProbedInsertion(rtn)
    )
    {
        PROTO proto_realloc = PROTO_Allocate( PIN_PARG(VOID *), CALLINGSTD_DEFAULT,
            REALLOC.c_str(), PIN_PARG(VOID *), PIN_PARG(UINT64), PIN_PARG_END() );
        RTN_ReplaceSignatureProbed(rtn, AFUNPTR(WrapRealloc),
            IARG_PROTOTYPE, proto_realloc,
            IARG_ORIG_FUNCPTR,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
            IARG_END);
        PROTO_Free( proto_realloc );
    }

    rtn = RTN_FindByName( img, FREE.c_str() );
    if (
        RTN_Valid(rtn) &&
        RTN_IsSafeForProbedInsertion(rtn)
    )
    {
        PROTO proto_free = PROTO_Allocate( PIN_PARG(VOID), CALLINGSTD_DEFAULT,
            FREE.c_str(), PIN_PARG(VOID *), PIN_PARG_END() );
        RTN_ReplaceSignatureProbed(rtn, AFUNPTR(WrapFree),
            IARG_PROTOTYPE, proto_free,
            IARG_ORIG_FUNCPTR,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
            IARG_END);
        PROTO_Free( proto_free );
    }

    rtn = RTN_FindByName( img, BACKTRACE.c_str() );
    if (
        RTN_Valid(rtn) &&
        RTN_IsSafeForProbedInsertion(rtn)
    )
    {
        ADDRINT addr = RTN_Address(rtn);
        g_backtrace = reinterpret_cast<BacktracePtr>(addr);
    }
}

/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */

INT32 main( INT32 argc, CHAR *argv[] )
{
    PIN_InitSymbols();

    if( PIN_Init(argc,argv) )
    {
        Help();
    }

    IMG_AddInstrumentFunction( ImageLoad, 0 );

    PIN_StartProgramProbed();

    return 0;
}

/* ===================================================================== */
/* eof */
/* ===================================================================== */
