#include <UDRefl/MethodPtr.h>

using namespace Ubpa::UDRefl;

ParamList::ParamList(std::vector<Parameter> params) :
	params{ params }
{
	offsets.reserve(params.size());

	size_t curOffset = 0;
	for (const auto& param : params) {
		if (param.alignment > alignment)
			alignment = param.alignment;

		curOffset = (curOffset + param.alignment - 1) / param.alignment;
		offsets.push_back(curOffset);
		curOffset += param.size;
	}

	size = curOffset;
}

bool ParamList::IsConpatibleWith(Span<TypeID> typeIDs) const noexcept {
	if (params.size() != typeIDs.size())
		return false;

	for (size_t i = 0; i < params.size(); i++) {
		if (params[i].typeID != typeIDs[i])
			return false;
	}

	return true;
}

bool ParamList::operator==(const ParamList& rhs) const noexcept {
	if (size != rhs.size)
		return false;
	if (alignment != rhs.alignment)
		return false;
	if (params.size() != rhs.params.size())
		return false;
	for (size_t i = 0; i < params.size(); i++) {
		if (params[i].typeID != rhs.params[i].typeID)
			return false;
	}
	return true;
}