/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "binary_encoder.h"

#include "shared/offline_compiler/source/offline_compiler.h"
#include "shared/source/device_binary_format/elf/elf_encoder.h"
#include "shared/source/device_binary_format/elf/ocl_elf.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/file_io.h"
#include "shared/source/helpers/hash.h"

#include "CL/cl.h"
#include "helper.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <sstream>

void BinaryEncoder::calculatePatchListSizes(std::vector<std::string> &ptmFile) {
    size_t patchListPos = 0;
    for (size_t i = 0; i < ptmFile.size(); ++i) {
        if (ptmFile[i].find("PatchListSize") != std::string::npos) {
            patchListPos = i;
        } else if (ptmFile[i].find("PATCH_TOKEN") != std::string::npos) {
            uint32_t calcSize = 0;
            i++;
            while (i < ptmFile.size() && ptmFile[i].find("Kernel #") == std::string::npos) {
                if (ptmFile[i].find(':') == std::string::npos) {
                    if (ptmFile[i].find("Hex") != std::string::npos) {
                        calcSize += static_cast<uint32_t>(std::count(ptmFile[i].begin(), ptmFile[i].end(), ' '));
                    } else {
                        calcSize += std::atoi(&ptmFile[i][1]);
                    }
                }
                i++;
            }
            uint32_t size = static_cast<uint32_t>(std::stoul(ptmFile[patchListPos].substr(ptmFile[patchListPos].find_last_of(' ') + 1)));
            if (size != calcSize) {
                argHelper->printf("Warning! Calculated PatchListSize ( %u ) differs from file ( %u ) - changing it. Line %d\n", calcSize, size, static_cast<int>(patchListPos + 1));
                ptmFile[patchListPos] = ptmFile[patchListPos].substr(0, ptmFile[patchListPos].find_last_of(' ') + 1);
                ptmFile[patchListPos] += std::to_string(calcSize);
            }
        }
    }
}

bool BinaryEncoder::copyBinaryToBinary(const std::string &srcFileName, std::ostream &outBinary, uint32_t *binaryLength) {
    if (!argHelper->fileExists(srcFileName)) {
        return false;
    }

    auto binary = argHelper->readBinaryFile(srcFileName);
    auto length = binary.size();
    outBinary.write(binary.data(), length);

    if (binaryLength) {
        *binaryLength = static_cast<uint32_t>(length);
    }

    return true;
}

int BinaryEncoder::createElf(std::stringstream &deviceBinary) {
    NEO::Elf::ElfEncoder<NEO::Elf::EI_CLASS_64> elfEncoder;
    elfEncoder.getElfFileHeader().type = NEO::Elf::ET_OPENCL_EXECUTABLE;
    // Build Options
    if (argHelper->fileExists(pathToDump + "build.bin")) {
        auto binary = argHelper->readBinaryFile(pathToDump + "build.bin");
        elfEncoder.appendSection(NEO::Elf::SHT_OPENCL_OPTIONS, "BuildOptions",
                                 ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(binary.data()), binary.size()));
    } else {
        argHelper->printf("Warning! Missing build section.\n");
    }
    // LLVM or SPIRV
    if (argHelper->fileExists(pathToDump + "llvm.bin")) {
        auto binary = argHelper->readBinaryFile(pathToDump + "llvm.bin");
        elfEncoder.appendSection(NEO::Elf::SHT_OPENCL_LLVM_BINARY, "Intel(R) OpenCL LLVM Object",
                                 ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(binary.data()), binary.size()));
    } else if (argHelper->fileExists(pathToDump + "spirv.bin")) {
        auto binary = argHelper->readBinaryFile(pathToDump + "spirv.bin");
        elfEncoder.appendSection(NEO::Elf::SHT_OPENCL_SPIRV, "SPIRV Object",
                                 ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(binary.data()), binary.size()));
    } else {
        argHelper->printf("Warning! Missing llvm/spirv section.\n");
    }

    // Device Binary
    auto deviceBinaryStr = deviceBinary.str();
    std::vector<char> binary(deviceBinaryStr.begin(), deviceBinaryStr.end());
    elfEncoder.appendSection(NEO::Elf::SHT_OPENCL_DEV_BINARY, "Intel(R) OpenCL Device Binary",
                             ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(binary.data()), binary.size()));

    // Resolve Elf Binary
    auto elfBinary = elfEncoder.encode();
    argHelper->saveOutput(elfName, elfBinary.data(), elfBinary.size());

    return 0;
}

void BinaryEncoder::printHelp() {
    argHelper->printf(R"===(Assembles Intel Compute GPU device binary from input files.
It's expected that input files were previously generated by 'ocloc disasm'
command or are compatible with 'ocloc disasm' output (especially in terms of
file naming scheme). See 'ocloc disasm --help' for additional info.

Usage: ocloc asm -out <out_file> [-dump <dump_dir>] [-device <device_type>] [-ignore_isa_padding]
  -out <out_file>           Filename for newly assembled binary.

  -dump <dumping_dir>       Path to the input directory containing
                            disassembled binary (as disassembled
                            by ocloc's disasm command).
                            Default is './dump'.

  -device <device_type>     Optional target device of output binary
                            <device_type> can be: %s
                            By default ocloc will pick base device within
                            a generation - i.e. both skl and kbl will
                            fallback to skl. If specific product (e.g. kbl)
                            is needed, provide it as device_type.

  -ignore_isa_padding       Ignores Kernel Heap padding - padding will not
                            be added to Kernel Heap binary.

  --help                    Print this usage message.

Examples:
  Assemble to Intel Compute GPU device binary
    ocloc asm -out reassembled.bin
)===",
                      argHelper->createStringForArgs(argHelper->productConfigHelper->getAllProductAcronyms()).c_str());
}

int BinaryEncoder::encode() {
    std::vector<std::string> ptmFile;
    if (!argHelper->fileExists(pathToDump + "PTM.txt")) {
        argHelper->printf("Error! Couldn't find PTM.txt");
        return -1;
    }
    argHelper->readFileToVectorOfStrings(pathToDump + "PTM.txt", ptmFile);

    calculatePatchListSizes(ptmFile);

    std::stringstream deviceBinary; //(pathToDump + "device_binary.bin", std::ios::binary);
    int retVal = processBinary(ptmFile, deviceBinary);
    argHelper->saveOutput(pathToDump + "device_binary.bin", deviceBinary.str().c_str(), deviceBinary.str().length());
    if (retVal != 0) {
        return retVal;
    }

    retVal = createElf(deviceBinary);
    return retVal;
}

int BinaryEncoder::processBinary(const std::vector<std::string> &ptmFileLines, std::ostream &deviceBinary) {
    if (false == iga->isKnownPlatform()) {
        auto deviceMarker = findPos(ptmFileLines, "Device");
        if (deviceMarker != ptmFileLines.size()) {
            std::stringstream ss(ptmFileLines[deviceMarker]);
            ss.ignore(32, ' ');
            ss.ignore(32, ' ');
            uint32_t gfxCore = 0;
            ss >> gfxCore;
            iga->setGfxCore(static_cast<GFXCORE_FAMILY>(gfxCore));
        }
    }
    size_t i = 0;
    while (i < ptmFileLines.size()) {
        if (ptmFileLines[i].find("Kernel #") != std::string::npos) {
            if (processKernel(++i, ptmFileLines, deviceBinary)) {
                argHelper->printf("Warning while processing kernel!\n");
                return -1;
            }
        } else if (writeDeviceBinary(ptmFileLines[i++], deviceBinary)) {
            argHelper->printf("Error while writing to binary!\n");
            return -1;
        }
    }
    return 0;
}

void BinaryEncoder::addPadding(std::ostream &out, size_t numBytes) {
    for (size_t i = 0; i < numBytes; ++i) {
        const char nullByte = 0;
        out.write(&nullByte, 1U);
    }
}

int BinaryEncoder::processKernel(size_t &line, const std::vector<std::string> &ptmFileLines, std::ostream &deviceBinary) {
    auto kernelInfoBeginMarker = line;
    auto kernelInfoEndMarker = ptmFileLines.size();
    auto kernelNameMarker = ptmFileLines.size();
    auto kernelPatchtokensMarker = ptmFileLines.size();
    std::stringstream kernelBlob;

    // Normally these are added by the compiler, need to take or of them when reassembling
    constexpr size_t isaPaddingSizeInBytes = 128;
    constexpr uint32_t kernelHeapAlignmentInBytes = 64;

    uint32_t kernelNameSizeInBinary = 0;
    std::string kernelName;

    //  Scan PTM lines for kernel info
    while (line < ptmFileLines.size()) {
        if (ptmFileLines[line].find("KernelName ") != std::string::npos) {
            kernelName = std::string(ptmFileLines[line], ptmFileLines[line].find(' ') + 1);
            kernelNameMarker = line;
            kernelPatchtokensMarker = kernelNameMarker + 1; // patchtokens come after name
        } else if (ptmFileLines[line].find("KernelNameSize") != std::string::npos) {
            std::stringstream ss(ptmFileLines[line]);
            ss.ignore(32, ' ');
            ss.ignore(32, ' ');
            ss >> kernelNameSizeInBinary;
        } else if (ptmFileLines[line].find("Kernel #") != std::string::npos) {
            kernelInfoEndMarker = line;
            break;
        }
        ++line;
    }

    // Write KernelName and padding
    kernelBlob.write(kernelName.c_str(), kernelName.size());
    addPadding(kernelBlob, kernelNameSizeInBinary - kernelName.size());

    // Write KernelHeap and padding
    uint32_t kernelHeapSizeUnpadded = 0U;
    bool heapsCopiedSuccesfully = true;

    // Use .asm if available, fallback to .dat
    if (argHelper->fileExists(pathToDump + kernelName + "_KernelHeap.asm")) {
        auto kernelAsAsm = argHelper->readBinaryFile(pathToDump + kernelName + "_KernelHeap.asm");
        std::string kernelAsBinary;
        argHelper->printf("Trying to assemble %s.asm\n", kernelName.c_str());
        if (false == iga->tryAssembleGenISA(std::string(kernelAsAsm.begin(), kernelAsAsm.end()), kernelAsBinary)) {
            argHelper->printf("Error : Could not assemble : %s\n", kernelName.c_str());
            return -1;
        }
        kernelHeapSizeUnpadded = static_cast<uint32_t>(kernelAsBinary.size());
        kernelBlob.write(kernelAsBinary.data(), kernelAsBinary.size());
    } else {
        heapsCopiedSuccesfully = copyBinaryToBinary(pathToDump + kernelName + "_KernelHeap.dat", kernelBlob, &kernelHeapSizeUnpadded);
    }

    uint32_t kernelHeapSize = 0U;
    // Adding padding and alignment
    if (ignoreIsaPadding) {
        kernelHeapSize = kernelHeapSizeUnpadded;
    } else {
        addPadding(kernelBlob, isaPaddingSizeInBytes);
        const uint32_t kernelHeapPaddedSize = kernelHeapSizeUnpadded + isaPaddingSizeInBytes;
        kernelHeapSize = alignUp(kernelHeapPaddedSize, kernelHeapAlignmentInBytes);
        addPadding(kernelBlob, kernelHeapSize - kernelHeapPaddedSize);
    }

    // Write GeneralStateHeap, DynamicStateHeap, SurfaceStateHeap
    if (argHelper->fileExists(pathToDump + kernelName + "_GeneralStateHeap.bin")) {
        heapsCopiedSuccesfully = heapsCopiedSuccesfully && copyBinaryToBinary(pathToDump + kernelName + "_GeneralStateHeap.bin", kernelBlob);
    }
    heapsCopiedSuccesfully = heapsCopiedSuccesfully && copyBinaryToBinary(pathToDump + kernelName + "_DynamicStateHeap.bin", kernelBlob);
    heapsCopiedSuccesfully = heapsCopiedSuccesfully && copyBinaryToBinary(pathToDump + kernelName + "_SurfaceStateHeap.bin", kernelBlob);
    if (false == heapsCopiedSuccesfully) {
        return -1;
    }

    // Write kernel patchtokens
    for (size_t i = kernelPatchtokensMarker; i < kernelInfoEndMarker; ++i) {
        if (writeDeviceBinary(ptmFileLines[i], kernelBlob)) {
            argHelper->printf("Error while writing to binary.\n");
            return -1;
        }
    }

    auto kernelBlobData = kernelBlob.str();
    uint64_t hashValue = NEO::Hash::hash(reinterpret_cast<const char *>(kernelBlobData.data()), kernelBlobData.size());
    uint32_t calcCheckSum = hashValue & 0xFFFFFFFF;

    // Add kernel header
    for (size_t i = kernelInfoBeginMarker; i < kernelNameMarker; ++i) {
        if (ptmFileLines[i].find("CheckSum") != std::string::npos) {
            static_assert(std::is_same<decltype(calcCheckSum), uint32_t>::value, "");
            deviceBinary.write(reinterpret_cast<char *>(&calcCheckSum), sizeof(uint32_t));
        } else if (ptmFileLines[i].find("KernelHeapSize") != std::string::npos) {
            static_assert(sizeof(kernelHeapSize) == sizeof(uint32_t), "");
            deviceBinary.write(reinterpret_cast<const char *>(&kernelHeapSize), sizeof(uint32_t));
        } else if (ptmFileLines[i].find("KernelUnpaddedSize") != std::string::npos) {
            static_assert(sizeof(kernelHeapSizeUnpadded) == sizeof(uint32_t), "");
            deviceBinary.write(reinterpret_cast<char *>(&kernelHeapSizeUnpadded), sizeof(uint32_t));
        } else {
            if (writeDeviceBinary(ptmFileLines[i], deviceBinary)) {
                argHelper->printf("Error while writing to binary.\n");
                return -1;
            }
        }
    }

    // Add kernel blob after the header
    deviceBinary.write(kernelBlobData.c_str(), kernelBlobData.size());

    return 0;
}

int BinaryEncoder::validateInput(const std::vector<std::string> &args) {
    for (size_t argIndex = 2; argIndex < args.size(); ++argIndex) {
        const auto &currArg = args[argIndex];
        const bool hasMoreArgs = (argIndex + 1 < args.size());
        if ("-dump" == currArg && hasMoreArgs) {
            pathToDump = args[++argIndex];
            addSlash(pathToDump);
        } else if ("-device" == currArg && hasMoreArgs) {
            iga->setProductFamily(getProductFamilyFromDeviceName(args[++argIndex]));
        } else if ("-out" == currArg && hasMoreArgs) {
            elfName = args[++argIndex];
        } else if ("--help" == currArg) {
            showHelp = true;
            return 0;
        } else if ("-ignore_isa_padding" == currArg) {
            ignoreIsaPadding = true;
        } else if ("-q" == currArg) {
            argHelper->getPrinterRef() = MessagePrinter(true);
            iga->setMessagePrinter(argHelper->getPrinterRef());
        } else {
            argHelper->printf("Unknown argument %s\n", currArg.c_str());
            return -1;
        }
    }
    if (pathToDump.empty()) {
        if (!argHelper->outputEnabled()) {
            argHelper->printf("Warning : Path to dump folder not specificed - using ./dump as default.\n");
            pathToDump = "dump";
            addSlash(pathToDump);
        }
    }
    if (elfName.find(".bin") == std::string::npos) {
        argHelper->printf(".bin extension is expected for binary file.\n");
        return -1;
    }

    if (false == iga->isKnownPlatform()) {
        argHelper->printf("Warning : missing or invalid -device parameter - results may be inacurate\n");
    }
    return 0;
}

template <typename T>
void BinaryEncoder::write(std::stringstream &in, std::ostream &deviceBinary) {
    T val;
    in >> val;
    deviceBinary.write(reinterpret_cast<const char *>(&val), sizeof(T));
}
template <>
void BinaryEncoder::write<uint8_t>(std::stringstream &in, std::ostream &deviceBinary) {
    uint8_t val;
    uint16_t help;
    in >> help;
    val = static_cast<uint8_t>(help);
    deviceBinary.write(reinterpret_cast<const char *>(&val), sizeof(uint8_t));
}
template void BinaryEncoder::write<uint16_t>(std::stringstream &in, std::ostream &deviceBinary);
template void BinaryEncoder::write<uint32_t>(std::stringstream &in, std::ostream &deviceBinary);
template void BinaryEncoder::write<uint64_t>(std::stringstream &in, std::ostream &deviceBinary);

int BinaryEncoder::writeDeviceBinary(const std::string &line, std::ostream &deviceBinary) {
    if (line.find(':') != std::string::npos) {
        return 0;
    } else if (line.find("Hex") != std::string::npos) {
        std::stringstream ss(line);
        ss.ignore(32, ' ');
        uint16_t tmp;
        uint8_t byte;
        while (!ss.eof()) {
            ss >> std::hex >> tmp;
            byte = static_cast<uint8_t>(tmp);
            deviceBinary.write(reinterpret_cast<const char *>(&byte), sizeof(uint8_t));
        }
    } else {
        std::stringstream ss(line);
        uint16_t size;
        std::string name;
        ss >> size;
        ss >> name;
        switch (size) {
        case 1:
            write<uint8_t>(ss, deviceBinary);
            break;
        case 2:
            write<uint16_t>(ss, deviceBinary);
            break;
        case 4:
            write<uint32_t>(ss, deviceBinary);
            break;
        case 8:
            write<uint64_t>(ss, deviceBinary);
            break;
        default:
            argHelper->printf("Unknown size in line: %s\n", line.c_str());
            return -1;
        }
    }
    return 0;
}
