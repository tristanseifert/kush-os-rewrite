#ifndef TASK_LOADER_LOADER_H
#define TASK_LOADER_LOADER_H

#include <cstddef>
#include <cstdint>
#include <exception>
#include <memory>
#include <span>
#include <string>
#include <string_view>

namespace task {
class Task;
}

namespace task::loader {
/**
 * Interface of a binary loader
 */
class Loader {
    public:
        Loader(const std::span<std::byte> &bytes) : file(bytes) {};
        virtual ~Loader() = default;

        /// Gets an identifier of this loader.
        virtual std::string_view getLoaderId() const = 0;

        /// Returns the address of the binary's entry point.
        virtual uintptr_t getEntryAddress() const = 0;
        /// Returns the virtual memory address of the bottom of the entry point stack
        virtual uintptr_t getStackBottomAddress() const = 0;
        /// If the dynamic linker needs to be notified we've been launched.
        virtual bool needsDyldoInsertion() const = 0;

        /// Maps the loadable sections of the ELF into the task.
        virtual void mapInto(std::shared_ptr<Task> &task) = 0;
        /// Sets up the entry point stack in the given task.
        virtual void setUpStack(std::shared_ptr<Task> &task) = 0;

    protected:
        /// in-memory copy of the ELF
        std::span<std::byte> file;
};

/**
 * Error surfaced during loading of a binary
 */
class LoaderError: public std::exception {
    public:
        LoaderError();
        LoaderError(const char *what) : whatStr(std::string(what)) {}
        LoaderError(const std::string &what) : whatStr(what) {}

        // returns the error string
        const char *what() const noexcept override {
            return this->whatStr.c_str();
        }

    private:
        /// error description
        std::string whatStr;
};
}

#endif
