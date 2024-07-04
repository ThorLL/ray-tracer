#pragma once
#include <vector>

struct ShaderStruct {
public:
	virtual ~ShaderStruct() = default;
	[[nodiscard]] virtual std::vector<std::byte> GetBytes() = 0;

private:
	static size_t AddSize() {return 0;}

	template<typename T, typename... Args>
	static size_t AddSize(T &, Args... args) {
		const size_t size = std::max(static_cast<int>(sizeof(T)), 4);
		return 4 * (size == 12) + size + AddSize(args...);
	}

	void AddBytes(std::byte*) {}

	template<typename T, typename... Args>
	void AddBytes(std::byte *bufferPtr, T first, Args... args) {
		std::memcpy(bufferPtr, &first, sizeof(T));
		const size_t size = std::max(static_cast<int>(sizeof(T)), 4);
		AddBytes(bufferPtr + 4 * (size == 12) + size, args...);
	}

protected:
	template<typename T, typename... Args>
	std::vector<std::byte> ConvertToBytes(T first, Args... args) {
		size_t size = AddSize(first, args...);
		size_t padding = (16 - size % 16) * (size % 16 > 0);
		std::vector<std::byte> bytes(size + padding);
		AddBytes(bytes.data(), first, args...);
		return bytes;
	}
};
