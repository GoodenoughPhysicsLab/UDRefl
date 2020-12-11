#pragma once

#include <array>

namespace Ubpa::UDRefl::details {
	template<typename ArgList>
	struct GenerateMethodPtr_Helper;
	template<typename... Args>
	struct GenerateMethodPtr_Helper<TypeList<Args...>> {
		static ParamList GenerateParamList(ReflMngr& mngr)
			noexcept(sizeof...(Args) == 0)
		{
			return mngr.GenerateParamList<Args...>();
		}

		template<auto funcptr, size_t... Ns>
		static constexpr auto GenerateFunction(std::index_sequence<Ns...>) noexcept {
			using FuncPtr = decltype(funcptr);
			using Traits = FuncTraits<decltype(funcptr)>;
			if constexpr (std::is_member_function_pointer_v<FuncPtr>) {
				using ObjPtr = std::conditional_t<Traits::is_const, const void*, void*>;
				constexpr auto wrapped_func = [](ObjPtr obj, ArgsView args, void* result_buffer) -> Destructor {
					assert(((args.GetParamList().GetParameters()[Ns].typeID == TypeID::of<Args>)&&...));
					assert(((args.GetParamList().GetParameters()[Ns].size == sizeof(type_buffer_decay_t<Args>))&&...));
					assert(((args.GetParamList().GetParameters()[Ns].alignment == alignof(type_buffer_decay_t<Args>))&&...));
					constexpr auto f = wrap_function<funcptr>();
					return f(obj, args.GetBuffer(), result_buffer);
				};
				constexpr auto decayed_wrapped_func = DecayLambda(wrapped_func);
				return decayed_wrapped_func;
			}
			else if constexpr (is_function_pointer_v<FuncPtr>) {
				constexpr auto wrapped_func = [](ArgsView args, void* result_buffer) -> Destructor {
					assert(((args.GetParamList().GetParameters()[Ns].typeID == TypeID::of<Args>)&&...));
					assert(((args.GetParamList().GetParameters()[Ns].size == sizeof(type_buffer_decay_t<Args>))&&...));
					assert(((args.GetParamList().GetParameters()[Ns].alignment == alignof(type_buffer_decay_t<Args>))&&...));
					constexpr auto f = wrap_function<funcptr>();
					return f(args.GetBuffer(), result_buffer);
				};
				constexpr auto decayed_wrapped_func = DecayLambda(wrapped_func);
				return decayed_wrapped_func;
			}
			else
				static_assert(false);
		}

		template<typename Func, size_t... Ns>
		static /*constexpr*/ auto GenerateMemberFunction(Func&& func, std::index_sequence<Ns...>) noexcept {
			using Traits = WrapFuncTraits<std::decay_t<Func>>;
			using ObjPtr = std::conditional_t<Traits::is_const, const void*, void*>;
			/*constexpr*/ auto wrapped_func =
				[f = std::forward<Func>(func)](ObjPtr obj, ArgsView args, void* result_buffer) mutable -> Destructor {
					assert(((args.GetParamList().GetParameters()[Ns].typeID == TypeID::of<Args>)&&...));
					assert(((args.GetParamList().GetParameters()[Ns].size == sizeof(type_buffer_decay_t<Args>))&&...));
					assert(((args.GetParamList().GetParameters()[Ns].alignment == alignof(type_buffer_decay_t<Args>))&&...));
					auto wrapped_f = wrap_member_function(std::forward<Func>(f));
					return wrapped_f(obj, args.GetBuffer(), result_buffer);
				};

			return std::function{ wrapped_func };
		}

		template<typename Func, size_t... Ns>
		static /*constexpr*/ auto GenerateStaticFunction(Func&& func, std::index_sequence<Ns...>) noexcept {
			/*constexpr*/ auto wrapped_func =
				[f = std::forward<Func>(func)](ArgsView args, void* result_buffer) mutable ->Destructor {
					assert(((args.GetParamList().GetParameters()[Ns].typeID == TypeID::of<Args>)&&...));
					assert(((args.GetParamList().GetParameters()[Ns].size == sizeof(type_buffer_decay_t<Args>))&&...));
					assert(((args.GetParamList().GetParameters()[Ns].alignment == alignof(type_buffer_decay_t<Args>))&&...));
					auto wrapped_f = wrap_static_function(std::forward<Func>(f));
					return wrapped_f(args.GetBuffer(), result_buffer);
				};
			return std::function{ wrapped_func };
		}
	};
};

namespace Ubpa::UDRefl {
	//
	// Factory
	////////////

	template<auto field_data>
	FieldPtr ReflMngr::GenerateFieldPtr() {
		using FieldData = decltype(field_data);
		if constexpr (std::is_pointer_v<FieldData>) {
			using Value = std::remove_pointer_t<FieldData>;
			using ConstFlag = std::bool_constant<std::is_const_v<Value>>;
			const TypeID ID = tregistry.Register<Value>();
			return {
				ID,
				field_data,
				ConstFlag{}
			};
		}
		else if constexpr (std::is_member_object_pointer_v<FieldData>) {
			using Traits = member_pointer_traits<FieldData>;
			using Object = typename Traits::object;
			using Value = typename Traits::value;
			const TypeID ID = tregistry.Register<Value>();
			using ConstFlag = std::bool_constant<std::is_const_v<Value>>;
			if constexpr (has_virtual_base_v<Object>) {
				return {
					ID,
					field_offsetor<field_data>(),
					ConstFlag{}
				};
			}
			else {
				return {
					ID,
					field_forward_offset_value(field_data),
					ConstFlag{}
				};
			}
		}
		else if constexpr (std::is_enum_v<FieldData>) {
			using Value = std::remove_pointer_t<FieldData>;
			const TypeID ID = tregistry.Register<Value>();
			const auto buffer = FieldPtr::ConvertToBuffer(field_data);
			return {
				ID,
				buffer
			};
		}
		else
			static_assert(false);
	}

	template<typename T>
	FieldPtr ReflMngr::GenerateFieldPtr(T&& data) {
		using RawT = std::decay_t<T>;
		static_assert(!std::is_same_v<RawT, size_t>);
		if constexpr (std::is_member_object_pointer_v<RawT>) {
			using Traits = member_pointer_traits<RawT>;
			using Object = typename Traits::object;
			using Value = typename Traits::value;
			const TypeID ID = tregistry.Register<Value>();
			using ConstFlag = std::bool_constant<std::is_const_v<Value>>;
			if constexpr (has_virtual_base_v<Object>) {
				return {
					ID,
					field_offsetor(data),
					ConstFlag{}
				};
			}
			else {
				return {
					ID,
					field_forward_offset_value(data),
					ConstFlag{}
				};
			}
		}
		else if constexpr (std::is_pointer_v<RawT> && !is_function_pointer_v<RawT> && std::is_void_v<std::remove_pointer_t<RawT>>) {
			using Value = std::remove_pointer_t<RawT>;
			return {
				tregistry.Register<Value>(),
				data,
				std::bool_constant<std::is_const_v<Value>>{}
			};
		}
		else if constexpr (std::is_enum_v<RawT>) {
			const TypeID ID = tregistry.Register<RawT>();
			const auto buffer = FieldPtr::ConvertToBuffer(data);
			return {
				ID,
				buffer
			};
		}
		else {
			using Traits = FuncTraits<RawT>;

			using ArgList = typename Traits::ArgList;
			static_assert(Length_v<ArgList> == 1);
			using ObjPtr = Front_t<ArgList>;
			static_assert(std::is_pointer_v<ObjPtr>);
			using Obj = std::remove_pointer_t<ObjPtr>;
			static_assert(!std::is_const_v<Obj>);

			using ValuePtr = typename Traits::Return;
			static_assert(std::is_pointer_v<ValuePtr>);
			using Value = std::remove_pointer_t<ValuePtr>;
			static_assert(!std::is_void_v<Value>);

			const TypeID ID = tregistry.Register<Value>();
			using ConstFlag = std::bool_constant<std::is_const_v<Value>>;

			auto offsetor = [f=std::forward<T>(data)](const void* obj) -> const void* {
				return f(const_cast<Obj*>(reinterpret_cast<const Obj*>(obj)));
			};

			return {
				ID,
				offsetor,
				ConstFlag{}
			};
		}
	}

	template<typename T, typename... Args>
	FieldPtr ReflMngr::GenerateDynamicFieldPtr(Args&&... args) {
		using RawT = std::decay_t<T>;
		if constexpr (FieldPtr::IsBufferable<RawT>()) {
			using MaybeConstBuffer = std::conditional_t<std::is_const_v<T>, const FieldPtr::Buffer, FieldPtr::Buffer>;
			MaybeConstBuffer buffer = FieldPtr::ConvertToBuffer(T{ std::forward<Args>(args)... });
			return FieldPtr{ TypeID::of<RawT>, buffer };
		}
		else {
			using MaybeConstSharedObject = std::conditional_t<std::is_const_v<T>, const SharedObject, SharedObject>;
			MaybeConstSharedObject obj = { TypeID::of<RawT>, MakeSharedBuffer<std::remove_cv_t<T>>(std::forward<Args>(args)...) };
			return FieldPtr{ obj };
		}
	}

	template<typename Return>
	ResultDesc ReflMngr::GenerateResultDesc() {
		if constexpr (!std::is_void_v<Return>) {
			using T = type_buffer_decay_t<Return>;
			return {
				tregistry.Register<Return>(),
				sizeof(T),
				alignof(T)
			};
		}
		else {
			return {
				tregistry.Register<void>(),
				0,
				0
			};
		}
	}

	template<typename... Params>
	ParamList ReflMngr::GenerateParamList() noexcept(sizeof...(Params) == 0) {
		if constexpr (sizeof...(Params) > 0)
			return ParamList{ { {tregistry.Register<Params>(), sizeof(type_buffer_decay_t<Params>), alignof(type_buffer_decay_t<Params>)}... } };
		else
			return {};
	}

	template<auto funcptr>
	MethodPtr ReflMngr::GenerateMethodPtr() {
		using FuncPtr = decltype(funcptr);
		using Traits = FuncTraits<decltype(funcptr)>;
		using ArgList = typename Traits::ArgList;
		using Return = typename Traits::Return;
		using Helper = details::GenerateMethodPtr_Helper<ArgList>;
		return {
			Helper::template GenerateFunction<funcptr>(std::make_index_sequence<Length_v<ArgList>>{}),
			GenerateResultDesc<Return>(),
			Helper::GenerateParamList(*this)
		};
	}

	template<typename T, typename... Args>
	MethodPtr ReflMngr::GenerateConstructorPtr() {
		return GenerateMemberMethodPtr([](T& obj, Args... args) {
			new(&obj)T{ std::forward<Args>(args)... };
		});
	}

	template<typename T>
	MethodPtr ReflMngr::GenerateDestructorPtr() {
		return GenerateMemberMethodPtr([](const T& obj) {
			if constexpr (!std::is_trivial_v<T>)
				obj.~T();
		});
	}

	template<typename Func>
	MethodPtr ReflMngr::GenerateMemberMethodPtr(Func&& func) {
		using Traits = details::WrapFuncTraits<std::decay_t<Func>>;
		using ArgList = typename Traits::ArgList;
		using Return = typename Traits::Return;
		using Helper = details::GenerateMethodPtr_Helper<ArgList>;
		return {
			Helper::template GenerateMemberFunction(std::forward<Func>(func), std::make_index_sequence<Length_v<ArgList>>{}),
			GenerateResultDesc<Return>(),
			Helper::GenerateParamList(*this)
		};
	}

	template<typename Func>
	MethodPtr ReflMngr::GenerateStaticMethodPtr(Func&& func) {
		using Traits = FuncTraits<std::decay_t<Func>>;
		using Return = typename Traits::Return;
		using ArgList = typename Traits::ArgList;
		using Helper = details::GenerateMethodPtr_Helper<ArgList>;
		return {
			Helper::template GenerateStaticFunction(std::forward<Func>(func), std::make_index_sequence<Length_v<ArgList>>{}),
			GenerateResultDesc<Return>(),
			Helper::GenerateParamList(*this)
		};
	}

	//
	// Modifier
	/////////////

	template<typename T, typename... Args>
	TypeID ReflMngr::RegisterTypePro(AttrSet attrs_ctor, AttrSet attrs_dtor) {
		return RegisterTypePro(type_name<T>().name, sizeof(T), alignof(T),
			{ GenerateConstructorPtr<T, Args...>() , std::move(attrs_ctor) },
			{ GenerateDestructorPtr<T>() , std::move(attrs_dtor) });
	}

	template<auto field_data>
	StrID ReflMngr::AddField(std::string_view name, AttrSet attrs) {
		using FieldData = decltype(field_data);
		if constexpr (std::is_enum_v<FieldData>) {
			return AddField(
				TypeID::of<FieldData>,
				name,
				{ GenerateFieldPtr<field_data>(), std::move(attrs) }
			);
		}
		else {
			return AddField(
				TypeID::of<member_pointer_traits_object<FieldData>>,
				name,
				{ GenerateFieldPtr<field_data>(), std::move(attrs) }
			);
		}
	}

	template<auto funcptr>
	StrID ReflMngr::AddMethod(std::string_view name, AttrSet attrs) {
		return AddMethod(
			TypeID::of<member_pointer_traits_object<decltype(funcptr)>>,
			name,
			{ GenerateMethodPtr<funcptr>(), std::move(attrs) }
		);
	}

	template<typename Func>
	StrID ReflMngr::AddMemberMethod(std::string_view name, Func&& func, AttrSet attrs) {
		return AddMethod(
			TypeID::of<typename details::WrapFuncTraits<std::decay_t<Func>>::Object>,
			name,
			{ GenerateMemberMethodPtr(std::forward<Func>(func)), std::move(attrs) }
		);
	}

	//
	// Invoke
	///////////

	template<typename... Args>
	bool ReflMngr::IsStaticInvocable(TypeID typeID, StrID methodID) const noexcept {
		std::array argTypeIDs = { TypeID::of<Args>... };
		return IsStaticInvocable(typeID, methodID, Span<TypeID>{argTypeIDs});
	}

	template<typename... Args>
	bool ReflMngr::IsConstInvocable(TypeID typeID, StrID methodID) const noexcept {
		std::array argTypeIDs = { TypeID::of<Args>... };
		return IsConstInvocable(typeID, methodID, Span<TypeID>{argTypeIDs});
	}

	template<typename... Args>
	bool ReflMngr::IsInvocable(TypeID typeID, StrID methodID) const noexcept {
		std::array argTypeIDs = { TypeID::of<Args>... };
		return IsInvocable(typeID, methodID, Span<TypeID>{argTypeIDs});
	}

	template<typename T>
	T ReflMngr::InvokeRet(TypeID typeID, StrID methodID, Span<TypeID> argTypeIDs, void* args_buffer) const {
		std::uint8_t result_buffer[sizeof(T)];
		auto result = Invoke(typeID, methodID, argTypeIDs, args_buffer, result_buffer);
		assert(result.resultID == TypeID::of<T>);
		return result.Move<T>(result_buffer);
	}

	template<typename T>
	T ReflMngr::InvokeRet(ConstObjectPtr obj, StrID methodID, Span<TypeID> argTypeIDs, void* args_buffer) const {
		std::uint8_t result_buffer[sizeof(T)];
		auto result = Invoke(obj, methodID, argTypeIDs, args_buffer, result_buffer);
		assert(result.resultID == TypeID::of<T>);
		return result.Move<T>(result_buffer);
	}

	template<typename T>
	T ReflMngr::InvokeRet(ObjectPtr obj, StrID methodID, Span<TypeID> argTypeIDs, void* args_buffer) const {
		std::uint8_t result_buffer[sizeof(T)];
		auto result = Invoke(obj, methodID, argTypeIDs, args_buffer, result_buffer);
		assert(result.resultID == TypeID::of<T>);
		return result.Move<T>(result_buffer);
	}

	template<typename... Args>
	InvokeResult ReflMngr::InvokeArgs(TypeID typeID, StrID methodID, void* result_buffer, Args... args) const {
		if constexpr (sizeof...(Args) > 0) {
			std::array argTypeIDs = { TypeID::of<Args>... };
			auto args_buffer = type_buffer_decay_as_tuple<Args...>(std::forward<Args>(args)...);
			return Invoke(typeID, methodID, Span<TypeID>{ argTypeIDs }, static_cast<void*>(&args_buffer), result_buffer);
		}
		else
			return Invoke(typeID, methodID, {}, nullptr, result_buffer);
	}

	template<typename... Args>
	InvokeResult ReflMngr::InvokeArgs(ConstObjectPtr obj, StrID methodID, void* result_buffer, Args... args) const {
		if constexpr (sizeof...(Args) > 0) {
			std::array argTypeIDs = { TypeID::of<Args>... };
			auto args_buffer = type_buffer_decay_as_tuple<Args...>(std::forward<Args>(args)...);
			return Invoke(obj, methodID, Span<TypeID>{ argTypeIDs }, static_cast<void*>(&args_buffer), result_buffer);
		}
		else
			return Invoke(obj, methodID, {}, nullptr, result_buffer);
	}

	template<typename... Args>
	InvokeResult ReflMngr::InvokeArgs(ObjectPtr obj, StrID methodID, void* result_buffer, Args... args) const {
		if constexpr (sizeof...(Args) > 0) {
			std::array argTypeIDs = { TypeID::of<Args>... };
			auto args_buffer = type_buffer_decay_as_tuple<Args...>(std::forward<Args>(args)...);
			return Invoke(obj, methodID, Span<TypeID>{ argTypeIDs }, static_cast<void*>(&args_buffer), result_buffer);
		}
		else
			return Invoke(obj, methodID, {}, nullptr, result_buffer);
	}

	template<typename T, typename... Args>
	T ReflMngr::Invoke(TypeID typeID, StrID methodID, Args... args) const {
		if constexpr (sizeof...(Args) > 0) {
			std::array argTypeIDs = { TypeID::of<Args>... };
			auto args_buffer = type_buffer_decay_as_tuple<Args...>(std::forward<Args>(args)...);
			return InvokeRet<T>(typeID, methodID, Span<TypeID>{ argTypeIDs }, static_cast<void*>(&args_buffer));
		}
		else
			return InvokeRet<T>(typeID, methodID);
	}

	template<typename T, typename... Args>
	T ReflMngr::Invoke(ConstObjectPtr obj, StrID methodID, Args... args) const {
		if constexpr (sizeof...(Args) > 0) {
			std::array argTypeIDs = { TypeID::of<Args>... };
			auto args_buffer = type_buffer_decay_as_tuple<Args...>(std::forward<Args>(args)...);
			return InvokeRet<T>(obj, methodID, Span<TypeID>{ argTypeIDs }, static_cast<void*>(&args_buffer));
		}
		else
			return InvokeRet<T>(obj, methodID);
	}

	template<typename T, typename... Args>
	T ReflMngr::Invoke(ObjectPtr obj, StrID methodID, Args... args) const {
		if constexpr (sizeof...(Args) > 0) {
			std::array argTypeIDs = { TypeID::of<Args>... };
			auto args_buffer = type_buffer_decay_as_tuple<Args...>(std::forward<Args>(args)...);
			return InvokeRet<T>(obj, methodID, Span<TypeID>{ argTypeIDs }, static_cast<void*>(&args_buffer));
		}
		else
			return InvokeRet<T>(obj, methodID);
	}

	//
	// Meta
	/////////

	template<typename... Args>
	bool ReflMngr::IsConstructible(TypeID typeID) const noexcept {
		std::array argTypeIDs = { TypeID::of<Args>... };
		return IsConstructible(typeID, Span<TypeID>{argTypeIDs});
	}

	template<typename... Args>
	bool ReflMngr::Construct(ObjectPtr obj, Args... args) const {
		if constexpr (sizeof...(Args) > 0) {
			std::array argTypeIDs = { TypeID::of<Args>... };
			auto args_buffer = type_buffer_decay_as_tuple<Args...>(std::forward<Args>(args)...);
			return Construct(obj, Span<TypeID>{ argTypeIDs }, static_cast<void*>(&args_buffer));
		}
		else
			return Construct(obj);
	}

	template<typename... Args>
	ObjectPtr ReflMngr::New(TypeID typeID, Args... args) const {
		if constexpr (sizeof...(Args) > 0) {
			std::array argTypeIDs = { TypeID::of<Args>... };
			auto args_buffer = type_buffer_decay_as_tuple<Args...>(std::forward<Args>(args)...);
			return New(typeID, Span<TypeID>{ argTypeIDs }, static_cast<void*>(&args_buffer));
		}
		else
			return New(typeID);
	}

	template<typename T, typename... Args>
	ObjectPtr ReflMngr::New(Args... args) {
		static_assert(!std::is_const_v<T> && !std::is_volatile_v<T> && !std::is_reference_v<T> && !std::is_enum_v<T>);
		if (!IsRegisteredType(TypeID::of<T>))
			RegisterTypePro<T, Args...>();
		AddMethod(TypeID::of<T>, StrIDRegistry::Meta::ctor, { GenerateConstructorPtr<T, Args...>() });
		return New(TypeID::of<T>, std::forward<Args>(args)...);
	}

	template<typename... Args>
	SharedObject ReflMngr::MakeShared(TypeID typeID, Args... args) const {
		if constexpr (sizeof...(Args) > 0) {
			std::array argTypeIDs = { TypeID::of<Args>... };
			auto args_buffer = type_buffer_decay_as_tuple<Args...>(std::forward<Args>(args)...);
			return MakeShared(typeID, Span<TypeID>{ argTypeIDs }, static_cast<void*>(&args_buffer));
		}
		else
			return MakeShared(typeID);
	}

	template<typename T, typename... Args>
	SharedObject ReflMngr::MakeShared(Args... args) {
		static_assert(!std::is_const_v<T> && !std::is_volatile_v<T> && !std::is_reference_v<T> && !std::is_enum_v<T>);
		if (!IsRegisteredType(TypeID::of<T>))
			RegisterTypePro<T, Args...>();
		AddMethod(TypeID::of<T>, StrIDRegistry::Meta::ctor, { GenerateConstructorPtr<T, Args...>() });
		return MakeShared(TypeID::of<T>, std::forward<Args>(args)...);
	}
}