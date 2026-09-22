#pragma once
#include <templates/block.h>
#include <templates/singleton.h>

namespace CE::Obj {
	template<typename T>
	struct Pool : AbstractManager<T>, Singleton_CTS<Pool<T>> {
		static_assert(std::is_class_v<T>, "Pool<T> must have a class for T");

		// retrieve/construct N objects
		template<typename... Args>
		std::vector<std::shared_ptr<T>> retrieve_objects(std::size_t N, Args... args);

		// retrieve a block to fit N objects in (memory will be in an unknown state)
		Block<T> retrieve_block(std::size_t N);

		// return constructed objects to the pool
		void return_objects(T* p, std::size_t length);

		// return a block to the pool
		void return_block(const Block<T> &returned);
	private:

		// retrieve a block of heap memory, maybe construct objects if we can
		template<typename... Args>
		[[nodiscard]] static Block<T> allocate(size_t length, Args... args);
	};


}

#include "pool.hpp"
