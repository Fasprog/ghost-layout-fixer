#pragma once

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace ghost::tests
{

inline std::string decodeUtf16LeFile(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    const std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (bytes.size() < 2 || static_cast<unsigned char>(bytes[0]) != 0xFF || static_cast<unsigned char>(bytes[1]) != 0xFE)
    {
        return {};
    }

    std::string decoded;
    decoded.reserve(bytes.size() / 2);
    for (std::size_t i = 2; i + 1 < bytes.size(); i += 2)
    {
        const unsigned char lo = static_cast<unsigned char>(bytes[i]);
        const unsigned char hi = static_cast<unsigned char>(bytes[i + 1]);
        if (hi == 0)
        {
            decoded.push_back(static_cast<char>(lo));
        }
    }

    return decoded;
}

inline bool expect(bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "[FAIL] " << message << '\n';
        return false;
    }
    return true;
}

inline std::size_t countCommandsContaining(
    const std::vector<std::string>& commands,
    const std::string& token)
{
    std::size_t count = 0;
    for (const std::string& command : commands)
    {
        if (command.find(token) != std::string::npos)
        {
            ++count;
        }
    }
    return count;
}

inline std::vector<std::string> commandSlice(
    const std::vector<std::string>& commands,
    std::size_t begin,
    std::size_t end)
{
    return std::vector<std::string>(
        commands.begin() + static_cast<std::ptrdiff_t>(begin),
        commands.begin() + static_cast<std::ptrdiff_t>(end));
}

} // namespace ghost::tests
