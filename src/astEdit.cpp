#pragma once

#include "Luau/Allocator.h"
#include "Luau/Ast.h"

namespace dlcomp::bundler::astEdit {

Luau::Location unknownLocation{};

// location is probably unnecesary, remove?
struct nilVarDeclaration {
	Luau::AstName name;
	Luau::Location location;
};

template <typename T>
Luau::AstArray<T> InitAstArray(Luau::Allocator& allocator, size_t size) {
	Luau::AstArray<T> result;
	result.size = size;
	result.data = static_cast<T*>(allocator.allocate(sizeof(T) * size));
	return result;
}

// this is directly copying AstArray<T> Parser::copy
// please reffer to that
// template <typename T>
// Luau::AstArray<T> CopyToAstArray(Luau::Allocator& allocator, T* data, size_t size) {
// 	Luau::AstArray<T> result = InitAstArray<T>(allocator, size);

// 	// This is equivalent to std::uninitialized_copy, but without the exception guarantee
// 	// since our types don't have destructors
// 	// ^ comment from luau devs
// 	for (size_t i = 0; i < size; ++i)
// 		new (result.data + i) T(data[i]);

// 	return result;
// }

template <typename T>
Luau::AstArray<T*> AstArrayPushFrontNoCopy(Luau::Allocator& allocator, Luau::AstArray<T*> existing, T* add) {
	size_t size = existing.size + 1;
	Luau::AstArray<T*> result = InitAstArray<T*>(allocator, size);

	result.data[0] = add;
	for (auto i = 0; i < existing.size; i++)
		result.data[i + 1] = existing.data[i];

	return result;
}

Luau::AstStatLocal* NodeAstStatLocal(Luau::Allocator& allocator, const std::vector<Luau::AstLocal*>& vars) {
	auto localNames = InitAstArray<Luau::AstLocal*>(allocator, vars.size());
	for (int i = 0; i < vars.size(); i++)
		localNames.data[i] = vars[i];

	return allocator.alloc<Luau::AstStatLocal>(unknownLocation, localNames, Luau::AstArray<Luau::AstExpr*>{}, std::nullopt);
}

} // namespace dlcomp::bundler::astEdit