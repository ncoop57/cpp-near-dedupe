#pragma once

#include "HasherThread.h"
#include "Hashing.h"
#include "LockableQueue.h"
#include "Jaccard.h"
#include <list>
#include <thread>

#include "ThreadPool.h"


struct CompareThreadDupeItem
{
    CompareThreadDupeItem(uint32_t _docId, int64_t _rowNumber)
        :docId(_docId),
        rowNumber(_rowNumber)
    {}

    uint32_t docId;
    int64_t rowNumber;
};


template<typename UINT_HASH_TYPE>
struct CompareItem
{
    CompareItem(HasherThreadOutputData< UINT_HASH_TYPE>* _myHashData, double _maxMatchedVal)
        :myHashData(_myHashData)
    {}

    ~CompareItem()
    {
        delete myHashData;
    }

    HasherThreadOutputData< UINT_HASH_TYPE>* myHashData;
};


//memory stuff
template<typename UINT_HASH_TYPE, uint32_t HASH_COUNT>
struct HashBlockEntry
{
    UINT_HASH_TYPE hashes[HASH_COUNT];
    uint64_t hashLen = 0;
};

template<typename UINT_HASH_TYPE, uint32_t HASH_COUNT, uint32_t BLOCK_SIZE>
struct Block
{
    UINT_HASH_TYPE size = 0;
    HashBlockEntry<UINT_HASH_TYPE, HASH_COUNT> entries[BLOCK_SIZE];

};


//just test with full blocks for now
template<typename UINT_HASH_TYPE, uint32_t MAX_HASH_LEN, uint32_t BLOCK_SIZE>
class HashBlockAllocator
{
    //last entry is where we add stuff...
    //first entry is set at start
    //second entry is in case we run out of room, so we restart allocating without
    //having to get a gargantuan contiguous array
    std::vector< std::vector< Block< UINT_HASH_TYPE, MAX_HASH_LEN, BLOCK_SIZE>* >* > blockVecs;
    bool empty = true;
    //std::vector< Block<UINT_HASH_TYPE, MAX_HASH_LEN / 2, BLOCK_SIZE>* > halfBLocks;

    //TODO; configurable
    const size_t backupBlockVecCapacity = 4096;

public:
    HashBlockAllocator(uint32_t initialCapacity)
    {
        std::vector< Block< UINT_HASH_TYPE, MAX_HASH_LEN, BLOCK_SIZE>* >* initBlockVec = new std::vector< Block< UINT_HASH_TYPE, MAX_HASH_LEN, BLOCK_SIZE>* >() { nullptr };
        initBlockVec.reserve(initialCapacity);
        //reserve and add first block to fill
        initBlockVec.push_back(new Block<UINT_HASH_TYPE, MAX_HASH_LEN, BLOCK_SIZE>());
        blockVecs.push_back&initBlockVec);
    }

    ~HashBlockAllocator()
    {
        for (auto blockVec : blockVecs)
        {
            for (auto block : blockVec)
            {
                delete block;
            }
        }
    }

    uint64_t NumEntries()
    {
        uint64_t entries = 0;
        for (auto blockVec : blockVecs)
        {
            entries += (blockVec.size() * BLOCK_SIZE) - BLOCK_SIZE + blockVec[blockVec.size() - 1]->size;
        }
        return entries;
    }

    size_t NumBlocks()
    {
        size_t nb = 0;
        for (auto blockVec : blockVecs)
        {
            nb += blockVec.size();
        }
        return nb;
    }

    uint64_t MemoryUsage()
    {
        uint64_t items = 0;
        for (auto blockVec : blockVecs)
        {
            items += uint64_t(blockVec.size() * BLOCK_SIZE) - BLOCK_SIZE + blockVec[blockVec.size() - 1]->size;            
        }
        items *= MAX_HASH_LEN;
        return items * (uint64_t)sizeof(UINT_HASH_TYPE);
    }

    bool IsEmpty()
    {
        return empty;
    }

    Block<UINT_HASH_TYPE, MAX_HASH_LEN, BLOCK_SIZE>* GetBlockPtr(uint32_t ind)
    {
        if(ind >= blockVecs[0]->size())
            return (*blockVecs[1])[ind];
        return (*blockVecs[0])[ind];
    }

    void AddCompareItem(CompareItem<UINT_HASH_TYPE>* citem)
    {
        AddItem(citem->myHashData->hashes.get(), citem->myHashData->hashLen);
    }

    void AddItem(UINT_HASH_TYPE* hashes, uint32_t len)
    {
        empty = false;
        
        std::vector< Block< UINT_HASH_TYPE, MAX_HASH_LEN, BLOCK_SIZE>* >* blockVec = blockVecs[0];
        if (blockVecs.size() > 1)
            blockVec = blockVec[blockVecs.size() - 1];


        Block<UINT_HASH_TYPE, MAX_HASH_LEN, BLOCK_SIZE>* b = (*blockVec)[blockVec.size() - 1];

#ifdef __GNUC__
        memcpy(&(b->entries[b->size].hashes), hashes, len * sizeof(UINT_HASH_TYPE));
#else
        memcpy_s( &(b->entries[b->size].hashes), MAX_HASH_LEN * sizeof(UINT_HASH_TYPE), hashes, len * sizeof(UINT_HASH_TYPE));
#endif
        b->entries[b->size].hashLen = len;

        b->size++;
        if (b->size == BLOCK_SIZE)
        {
            if (blockVec->size() == blockVecs.capacity())
            {
                //we went over the expected memory usage, create a new block vec
                std::vector< Block< UINT_HASH_TYPE, MAX_HASH_LEN, BLOCK_SIZE>* >* newBlockVec = new std::vector< Block< UINT_HASH_TYPE, MAX_HASH_LEN, BLOCK_SIZE>* >() { nullptr };
                newBlockVec->reserve(backupBlockVecCapacity);
                newBlockVec->push_back(new Block<UINT_HASH_TYPE, MAX_HASH_LEN, BLOCK_SIZE>())
                blockVecs.push_back(newBlockVec);
            }
            else
            {
                blockVec.push_back(new Block<UINT_HASH_TYPE, MAX_HASH_LEN, BLOCK_SIZE>());
            }           
        }
    }
};


template<typename UINT_HASH_TYPE, uint32_t MAX_HASH_LEN, uint32_t BLOCK_SIZE>
bool WorkThreadFunc(
    std::stop_source workerThreadStopper,
    HashBlockAllocator<UINT_HASH_TYPE, MAX_HASH_LEN, BLOCK_SIZE>& hashblocks, uint32_t inclusiveStartInd, uint32_t exclusiveEndInd,
    double earlyOut, double dupeThreash, CompareItem<UINT_HASH_TYPE>* citem)
{
    //compare incoming against all others, update the its max value.
    //this will prioritize removing later documents that match, not the first one
    for (uint32_t blockInd = inclusiveStartInd; blockInd < exclusiveEndInd; blockInd++)
    {
        Block<UINT_HASH_TYPE, MAX_HASH_LEN, BLOCK_SIZE>* block = hashblocks.GetBlockPtr(blockInd);

        for (uint32_t hashInd = 0; hashInd < block->size && !workerThreadStopper.stop_requested(); ++hashInd)
        {
            double match = JaccardTurbo(citem->myHashData->hashes.get(), citem->myHashData->hashLen,
                &(block->entries[hashInd].hashes[0]), (int)(block->entries[hashInd].hashLen),
                earlyOut);

            if (match >= dupeThreash)
            {
                //we are done
                workerThreadStopper.request_stop();
                return true;
            }
        }
    }
    return false;
}


template<typename UINT_HASH_TYPE, uint32_t MAX_HASH_LEN, uint32_t BLOCK_SIZE>
class ComparerThread
{
protected:
    std::stop_source m_stop;
    std::atomic<uint32_t> maxThreadWorkers;
    BS::thread_pool* threadPool;
    uint32_t workChunkSize;


    LockableQueue< CompareThreadDupeItem* > duplicateItems;

    HashBlockAllocator<UINT_HASH_TYPE, MAX_HASH_LEN, BLOCK_SIZE> hashblocks;

    //TODO make this configurable
    uint32_t expectedBlockAllocations;

public:
    ComparerThread(uint32_t _workChunkSize, BS::thread_pool* _threadPool, uint32_t maxExpectedDocs, uint32_t maxThreadWorkers)
        : maxThreadWorkers(maxThreadWorkers),
        threadPool(_threadPool),
        workChunkSize(_workChunkSize),
        expectedBlockAllocations((maxExpectedDocs / BLOCK_SIZE) + BLOCK_SIZ),
        hashblocks(HashBlockAllocator<UINT_HASH_TYPE, MAX_HASH_LEN, BLOCK_SIZE>(expectedBlockAllocations))
    {
    }

    ~ComparerThread()
    {
    }

    void IncreaseMaxWorkerThreads(int amt)
    {
        maxThreadWorkers.fetch_add(amt);
    }

    uint32_t GetWorkerThreadCount()
    {
        return maxThreadWorkers.load();
    }

    void WaitForFinish()
    {
        m_stop.request_stop();
    }

    size_t GetUniqueItemsCount()
    {
        return hashblocks.NumEntries();
    }

    uint64_t GetMemUsageMB()
    {
        return hashblocks.MemoryUsage() / (1024ULL * 1024ULL);
    }

    LockableQueue< CompareThreadDupeItem* >* GetOutputQueuePtr()
    {
        return &duplicateItems;
    }

    void EnterProcFunc(LockableQueue< HasherThreadOutputData<UINT_HASH_TYPE>* >* hashedDataQueue, double earlyOut, double dupeThreash)
    {
        //this guy needs to compare each incoming hashed data against all prexisting data, gonna be slow.
        std::queue<HasherThreadOutputData<UINT_HASH_TYPE>* > workQueue;
        HasherThreadOutputData< UINT_HASH_TYPE>* workItem;

        while (!m_stop.stop_requested() || hashedDataQueue->Length() > 0)
        {
            if (hashedDataQueue->try_pop_range(&workQueue, workChunkSize, 10ms) == 0)
            {
                std::this_thread::sleep_for(25ms);
                continue;
            }

            while (workQueue.size() > 0)
            {
                workItem = workQueue.front();
                workQueue.pop();

                //early out since no checks
                if (hashblocks.IsEmpty()) [[unlikely]]
                {
                    hashblocks.AddItem(workItem->hashes.get(), workItem->hashLen);
                    delete workItem;
                    workItem = nullptr;
                    continue;
                }

                    //spread the work of comparing across threads..  
                uint32_t threadsToUse = maxThreadWorkers.load();
                BS::multi_future<bool> internalCompareThreadFutures;

                //parallelize across blocks, one item at a time
                //this will be at most off by numberOfThreads - 1, so we will add more to the ranges in the loop below
                uint32_t totalBlocks = (uint32_t)hashblocks.NumBlocks();
                uint32_t blocksPerThread = totalBlocks / threadsToUse;
                uint32_t inclusiveStartInd = 0;
                uint32_t exclusiveEndInd = blocksPerThread;

                uint32_t kickedOffBlocks = 0;
                uint32_t remainingThreads = threadsToUse;

                std::stop_source workerThreadStopper;

                CompareItem< UINT_HASH_TYPE>* citem = new CompareItem< UINT_HASH_TYPE>(std::move(workItem), 0.0);

                do
                {
                    internalCompareThreadFutures.push_back(
                        threadPool->submit([this, workerThreadStopper, inclusiveStartInd, exclusiveEndInd, &earlyOut, &dupeThreash, citem]() {

                            return WorkThreadFunc<UINT_HASH_TYPE, MAX_HASH_LEN, BLOCK_SIZE>(workerThreadStopper, hashblocks,
                                                                                            inclusiveStartInd, exclusiveEndInd, earlyOut, dupeThreash, citem);
                            }
                        )
                    );

                    inclusiveStartInd += blocksPerThread;
                    
                    remainingThreads--;
                    kickedOffBlocks += exclusiveEndInd;

                    exclusiveEndInd += blocksPerThread;

                    //if we arent gonna get all the blocks, increase the number we send to the nesxt thread by 1. 
                    if (kickedOffBlocks + (remainingThreads * blocksPerThread) < totalBlocks)
                        ++exclusiveEndInd;

                    if (hashblocks.NumBlocks() <= exclusiveEndInd)
                        exclusiveEndInd = (uint32_t)hashblocks.NumBlocks();

                } while (inclusiveStartInd < exclusiveEndInd);

                //wait for worker threads
                internalCompareThreadFutures.wait();

                //run through each and look at result, if its a dupe, pass it to dupes, if its not, we need to compare all the
                bool isDupe = false;
                for (size_t i = 0; i < internalCompareThreadFutures.size(); ++i)
                {
                    bool result = internalCompareThreadFutures[i].get();
                    if (result == true)
                    {
                        isDupe = true;
                        break;
                    }
                }

                if (isDupe)
                {
                    //for processing in the removal of dupes
                    CompareThreadDupeItem* dupeItem = new CompareThreadDupeItem(citem->myHashData->arrowData->docId, citem->myHashData->arrowData->rowNumber);
                    duplicateItems.push(std::move(dupeItem));
                }
                else
                {
                    hashblocks.AddCompareItem(citem);
                }

                delete citem;
                citem = nullptr;

            }//do work

        }//thread running

    }//thread func
};