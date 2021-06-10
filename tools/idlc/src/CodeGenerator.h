#ifndef CODEGENERATOR_H
#define CODEGENERATOR_H

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

#include "InterfaceDescription.h"

/**
 * Encapsulates the code generation for the Cap'n Proto structs used as part of the wire format of
 * the messages, as well as the C++ server and client stubs.
 *
 * For each interface, you will create an instance of the code generator.
 */
class CodeGenerator {
    using IDPointer = std::shared_ptr<InterfaceDescription>;
    using Argument = InterfaceDescription::Argument;
    using Method = InterfaceDescription::Method;

    public:
        /// namespace in which all protocol definitions live
        constexpr static const std::string_view kProtoNamespace{"rpc::_proto::messages"};

    public:
        CodeGenerator(const std::filesystem::path &outDir, const IDPointer &interface);

        /// Generates the Cap'n Proto messages for each method's params and reply.
        void generateProto();
        /// Generates the server stub for the interface
        void generateServerStub();

    private:
        void protoWriteMethod(std::ofstream &, const Method &);
        void protoWriteArgs(std::ofstream &, const std::vector<Argument> &);

        static std::string ProtoTypenameForArg(const Argument &);

        void serverWriteInfoBlock(std::ofstream &);
        void serverWriteHeader(std::ofstream &);

        void serverWriteImpl(std::ofstream &);
        void serverWriteMarshallMethod(std::ofstream &, const Method &);
        void serverWriteMarshallMethodReply(std::ofstream &, const Method &);

        void serverWriteMethodDef(std::ofstream &, const Method &, const std::string &prefix = "");
        static std::string CppTypenameForArg(const Argument &);

    private:
        // mapping of the type names defined in the IDL to Cap'n Proto names
        static const std::unordered_map<std::string, std::string> gProtoTypeNames;
        // mapping of the type names defined in the IDL to C++ type names
        static const std::unordered_map<std::string, std::string> gCppTypeNames;

        // timestamp for generation (ins ISO 8601 format)
        std::string creationTimestamp;

        // this is the interface for which we're generating code
        IDPointer interface;
        // directory into which output files are written
        std::filesystem::path outDir;

        // filename for the Cap'n Proto file
        std::filesystem::path protoFileName;
};

#endif