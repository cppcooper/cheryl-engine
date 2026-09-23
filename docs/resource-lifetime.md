# Resource lifetime and reservation

`Block<T>` remains the typed range and backing ownership primitive. `BlockManagement<T>` stores its pool, sections, registry, stale entries, and release queue in a shared `State`. Existing static accessors refer to this state. An `Obj::Pool<T>` facade holds a `shared_ptr<PoolState<T>>`; that state keeps the bookkeeping alive. An object handle captures the `PoolState` itself, so it can return storage after the facade dies without calling `Pool<T>::get()`.

`ObjectReservation<T, Allocator>` accepts an allocator only if its manager identifies a release context derived from `AbstractManager<T>` and exposes block retrieval, subrange return, and retained release operations. Currently `ObjectPoolAllocator<T>` satisfies that contract. `DefaultAllocator<T>`, `ObjectAllocator<T>`, and `std::allocator<T>` do not. The ordinary allocator `allocate(n)` and `deallocate(original,n)` still address a whole allocation. Reservations return interior ranges through their manager context; the generic factory now deallocates its one allocation only after the last element handle dies.

A reservation stores only unclaimed `Block<T>` ranges. `emplace(index, args...)` prepares split blocks and the handle before constructing the object, then transfers the one slot to the handle. If construction throws, the reservation still owns that slot. At destruction it returns the remaining contiguous ranges, while claimed object handles can outlive it. Trusted release methods used by destructors are `noexcept`: an internal invariant violation terminates rather than throwing from a deleter. Public return methods continue to report invalid calls with exceptions.

Underlying `Mem::ObjMMgr<T>` instances can die before the pool's last backing owner. The existing lifetime token causes that owner's callback to skip reuse bookkeeping; its captured `HeapBlock` still owns and frees the bytes. Grid and STB font geometry handles follow the same rule through `make_managed_block`.

The protected legacy `AssetMgr::allocate` interface remains available for callers that construct raw slots themselves. Sprite and tileset loaders now use reservations and create handles only for entries they actually construct. `Pool<T>::retrieve_objects` retains pool state in its element deleters; it no longer looks up a singleton when those handles die.

If construction of a `retrieve_objects` batch fails, completed handles release their slots and the unconstructed tail is returned as one range. `ObjCtor` reserves its tracking entry before invoking a constructor, so tracking allocation cannot fail after the object becomes live.

GPU lifetime remains separate: `GLSLProgram::~GLSLProgram` currently calls `glDeleteProgram` and requires a current context; VAO/VBO deletion and `Texture` deletion still need a context owned release queue. These operations were not changed as part of the CPU resource lifetime work.
