#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string>
#include <vector>

#define READ 0
#define READ_WRITE 1
#define EXECUTE_READ 2
#define EXECUTE_READ_WRITE 3


struct SegmentInfo{
    /**
     * @brief full segment name, if is present
     * 
     */
    std::string name;
    /**
     * @brief segment size range protection
     * 
     */
    std::string prot;
    /**
     * @brief entry of the the segment in memory
     * 
     */
    uintptr_t start;
    /**
     * @brief end of the segment in memory
     * 
     */
    uintptr_t end;
    /**
     * @brief segment size in memory
     * 
     */
    uintptr_t size;
};

class ProcessManager
{
private:
    /**
     * @brief targets process identifier(PID) 
     * 
     */
    int pid;

    /**
     * @brief target process memory file descriptor
     * 
     */
    int memfd;

    /**
     * @brief target process maps file descriptor
     * 
     */
    int mapsfd;


public:
    /**
     * @brief Construct a new Process Manager object
     *  
     * @param procName the name of the target process to attach.
     */
    ProcessManager(const char* procName);

    /**
     * @brief will find a proc id, enumerating all the /proc folders(pids).
     * 
     * @param procName its the name of the process that we want to get the pid
     * @return int 
     */
    static int FindPid(const char* procName);

    /**
     * @brief read the memory in target address
     * 
     * @tparam T target object type in target memory
     * @param addr it the target address
     * @return T target object return type from this function
     */
    template<typename T>
    T ReadProcessMemory(uintptr_t addr);

    /**
     * @brief solve multilevel pointers
     * 
     * @param base base address to start the multilevel pointer calculation
     * @param offsets array of offset to index the multilevel pointer
     * @return uintptr_t return the address pointed in the multilevel pointer provided
     */
    uintptr_t FindDMAddy(uintptr_t base, std::vector<uintptr_t> offsets);

    /**
     * @brief write the memory in target address
     * 
     * @tparam T target object type to be writed in target process memory
     * @param addr target address in process target memory
     * @param newValue target value to be write in target process memory
     */
    template<typename T>
    void WriteProcessMemory(uintptr_t addr, T newValue);

    /**
     * @brief Get the Module Base Address of a module loaded in target process memory
     * 
     * @param modName its the name of the module in target memory
     * @return uintptr_t the base address of the target module in the target memory 
     */
    uintptr_t GetModBaseAddr(const char* modName);

    /**
     * @brief Get the Module Base Address of a module loaded in local process memory
     * 
     * @param modName its the name of the module in local memory
     * @return uintptr_t the base address of the target module in the local memory 
     */
    static uintptr_t GetLocalModBaseAddr(const char* modName);

    /**
     * @brief will get the full module path in disk from a loaded module in target memory
     * 
     * @param modName name of the target module in target memory
     * @param result contains the full path of the target module
     * @return true result contain the full path
     * @return false an error ocurred while getting the full path, may wrong target library in target memory name
     */
    bool GetFullModulePath(const char* modName, std::string & result);

    /**
     * @brief will copy data from local memory to target process memory
     * 
     * @param source here is the source bytes in local memory
     * @param destination here is the destination point in target memory
     * @param size this is the count in bytes to be write
     */
    void memcpy(unsigned char* source, uintptr_t destination, int size);

    /**
     * @brief will copy data from target memory to local process memory
     * 
     * @param source here is the source bytes in target memory
     * @param destination here is the destination point in local memory
     * @param size this is the count in bytes to be write
     * @return true sucesfully copied
     * @return false error while copying
     */
    bool memcpyBackwrd(uintptr_t source, unsigned char* destination, int size);

    /**
     * @brief will find for a symbol in target memory
     * 
     * @param modName the name of the target module
     * @param symbolName the name of the target symbol
     * @return uintptr_t if not null an address to the symbol in target memory
     */
    uintptr_t FindExternalSymbol(const char* modName, const char* symbolName);

    /**
     * @brief it will enumerate/parse all segments from the maps file
     * 
     * @param segments this is a reference to a vector of segmentInfo struct
     * @param prot this is the target protection, could be READ, READ_WRITE, EXECUTE_READ, READ_WRITE_EXECUTE
     * @return true sucessfully enumerate all the target proteccion segments
     * @return false an error ocurred while enumerating
     */
    bool EnumSegments(std::vector<SegmentInfo> & segments, int prot);

    /**
     * @brief will find a piece of memory "empty" in the target process memory
     * 
     * @param size the target size of the "empty" memory
     * @param prot the target memory proteccion, could be READ, READ_WRITE, EXECUTE_READ, READ_WRITE_EXECUTE
     * @return uintptr_t if success the returned address is not null
     */
    uintptr_t FindCodeCave(uintptr_t size, uintptr_t prot);

    /**
     * @brief it will find and disable the ptrace function in the target process
     * 
     */
    void DisablePtrace();

    /**
     * @brief it will detour the execution flow in determinated target memory point
     * 
     * @param src where the detour will be aplied in the target memory
     * @param dst where the detour will go after detour in the target memory
     * @param size if arm, dont pass argument, the default size is 8, but in x86 need the size
     * @return true sucess  
     * @return false non sucess 
     */
    bool Hook(uintptr_t src, uintptr_t dst, uintptr_t targetLen = 8);
    
    /**
     * @brief will load a function located on local process memory in the target process memory and will make a detour from target process memory point to it
     * 
     * @param targetSrc detour entry
     * @param targetDst destination function in local process memory
     * @param targetLen lenght of bytes to be overwrited
     * @return true sucess
     * @return false fail
     */
    bool LoadToMemoryAndHook(uintptr_t targetSrc, void* targetDst, uintptr_t targetLen = 8);
};

template<typename T>
T ProcessManager::ReadProcessMemory(uintptr_t addr)
{
    T result;
    lseek(memfd, addr, SEEK_SET);

    if(!read(memfd, &result, sizeof(result)))
        printf("ProcessManager : could not read the memory");

    lseek(memfd, 0, SEEK_SET);
    return result;
}

template<typename T>
void ProcessManager::WriteProcessMemory(uintptr_t addr, T newValue)
{
    lseek(memfd, addr, SEEK_SET);

    if(!write(memfd, &newValue, sizeof(T)))
        printf("ProcessManager : could not write the memory");

    lseek(memfd, 0, SEEK_SET);
}


