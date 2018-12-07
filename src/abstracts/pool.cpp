#include "abstracts/pool.h"

/*
*/
closed_iter PoolAbstract::find_closed(void* p){
    /* Get first pair with a key not less than p
    -p is a masterptr
        search will land directly on the right pair
    -p is not a master pointer
        -p is not the head of an allocation
            must find the head
        -p is the head of an allocation
            must match p to pair.second.head
    */
    if(!ClosedList.empty()){
        auto iter = ClosedList.find(p);
        if(iter!=ClosedList.end()){
            //we found an exact match
            return iter;
        }
        iter = ClosedList.lower_bound(p);
        if(iter!=ClosedList.begin()){
            //we found something not less than what we're looking for
            --iter;
            void* mptr = iter->first; //p should mptr < p < mptr+master_size
            if(mptr < p && p < mptr+iter->second.master_size){
                //we just need to do a linear search now to find the right sub-allocation
                while(true){
                    if(mptr != iter->first){
                        //we've gone too far
                        return ClosedList.end();
                    }
                    alloc a = iter->second;
                    if(p == a.head){
                        return iter;
                    } else if (a.head < p && p < a.head+a.head_size){
                        //we found the sub-allocation that p is a sub-allocation of
                        return iter;
                    }
                    if(iter==ClosedList.begin()){
                        break;
                    }
                    --iter;
                }
            }
        }
    }
    return ClosedList.end();
}

/*
*/
void PoolAbstract::add_open(const alloc &a){
    auto result = OpenAllocations.emplace(a.head,a);
    if(!result.second){
        throw bad_request(__CEFUNCTION__,__LINE__,"Allocation already exists in OpenAllocations");
    }
    if(!OpenList.emplace(a.head_size,result.first).second){
        throw bad_request(__CEFUNCTION__,__LINE__,"Allocation already exists in OpenList");
    }
}

/*
*/
void PoolAbstract::erase_open(openlist_iter &iter){
    if(iter != OpenList.end()){
        OpenAllocations.erase(iter->second);
        OpenList.erase(iter);
    }
    throw bad_request(__CEFUNCTION__,__LINE__,"")
}

/*
*/
void PoolAbstract::erase_open(open_iter &iter){
    if(iter != OpenList.end()){
        alloc a = iter->second;
        auto iter2 = find_open(a);
        if(iter2==OpenList.end()){
            throw bad_request(__CEFUNCTION__,__LINE__,"Cannot find the OpenList iterator");
        }
        OpenAllocations.erase(iter);
        OpenList.erase(iter2);
    }
    throw invalid_args(__CEFUNCTION__,__LINE__);
}

/* protected method
    moves an allocation from the ClosedList to the OpenList
*/
void PoolAbstract::moveto_open(closed_iter iter, size_t N){
    alloc a = iter->second->second; //<x,<a,b>::iterator>::iterator
    size_t remainder = 0;
    if(N <= a.head_size){
        remainder = a.head_size - N;
        a.head_size = N;
        m_used -= N;
        m_free += N;
    } else {
        throw invalid_args(__CEFUNCTION__, __LINE__, "The elements returned must be less than or equal to the bytes allocated.");
    }
    ClosedList.erase(iter);
    auto existing = find_neighbours(a.head);
    if(existing.first != OpenAllocations.end()){
        alloc b = existing.first->second;
        if(a.head-b.head == b.head_size){
            auto iter2 = find_open(b);
            if(iter2 == OpenList.end()){
                throw bad_request(__CEFUNCTION__, __LINE__, "Allocation should exist, but was not found.");
            }
            erase_open(iter2);
            a.head = b.head;
            a.head_size += b.head_size;
        }
    }
    if(remainder>0){
        //if there is a remainder, we know there won't be a trailing allocation
        add_open(a);
        a.head += a.head_size;
        a.head_size = remainder;
        ClosedList.emplace(a.master,a);
    } else if(existing.second != OpenAllocations.end()){
        alloc c = existing.second->second;
        if(c.head-a.head == a.head_size){
            auto iter2 = find_open(c);
            if(iter2 == OpenList.end()){
                throw bad_request(__CEFUNCTION__, __LINE__, "Allocation should exist, but was not found.");
            }
            erase_open(iter2);
            a.head_size += c.head_size;
        }
        add_open(a);
    }
}

/*
*/
openlist_iter PoolAbstract::find_open(const alloc &a){
    auto iter = OpenList.lower_bound(a.head_size);
    for(; iter != OpenList.end(); ++iter){
        alloc &v = iter->second->second;
        if(v.head_size != a.head_size){
            return OpenList.end();
        } else if(v.head == a.head){
            return iter;
        }
    }
    return OpenList.end();
}

/*
*/
neighbours PoolAbstract::find_neighbours(void* p){
    auto iter2 = OpenAllocations.lower_bound(p);
    auto iter1 = iter2;
    if(iter1 != OpenAllocations.begin()){
        --iter1;
    }
    return std::make_pair(iter1,iter2);
}