#pragma once
#include <vector>

struct ShaderStruct {
	virtual ~ShaderStruct() = default;
	[[nodiscard]] virtual std::vector<std::byte> GetBytes() = 0;

	template <typename T, typename = std::enable_if_t<std::is_base_of_v<ShaderStruct, T> && !std::is_same_v<ShaderStruct, T>>>
	static size_t GetSize(T s) {
		return s.GetSize();
	}
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
		std::vector<std::byte> bytes(GetSize(first, args...));
		AddBytes(bytes.data(), first, args...);
		return bytes;
	}
	template<typename T, typename... Args>
	size_t GetSize(T first, Args... args) {
		const size_t size = AddSize(first, args...);
		return size +  (16 - size % 16) * (size % 16 > 0);
	}
};
