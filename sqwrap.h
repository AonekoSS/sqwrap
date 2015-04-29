//===========================================================================
//
//  sqwrap - simple Squirrel binder with function overload.
//
//===========================================================================
#include <limits>
#include <string>
#include <sstream>
#include <exception>
#include <assert.h>

#include <squirrel.h>

namespace sqwrap{
namespace detail{
	typedef const SQChar* SQcstr;
	typedef std::basic_string<SQChar> SQstring;
	typedef std::basic_stringstream<SQChar> SQsstream;

	SQInteger newtable(HSQUIRRELVM vm){
		sq_newtable(vm);
		return sq_gettop(vm);
	}
	SQInteger newarray(HSQUIRRELVM vm, SQInteger size){
		sq_newarray(vm, size);
		return sq_gettop(vm);
	}


//---------------------------------------------------------------------------
//	型特性
//---------------------------------------------------------------------------
	template <class T,
		bool N=std::numeric_limits<T>::is_specialized,
		bool I=std::numeric_limits<T>::is_integer>
	struct X{};

	// Integer
	template <class T>
	struct X<T, true, true>{
		static const char mask = 'n';
		static const char tag  = 'i';
		static void push(HSQUIRRELVM vm, T val){ sq_pushinteger(vm, val); }
		static T get(HSQUIRRELVM vm, SQInteger index){
			SQInteger i;
			sq_getinteger(vm, index, &i);
			return static_cast<T>(i);
		}
	};

	// Float
	template <class T>
	struct X<T, true, false>{
		static const char mask = 'n';
		static const char tag  = 'f';
		static void push(HSQUIRRELVM vm, T val){ sq_pushfloat(vm, val); }
		static T get(HSQUIRRELVM vm, SQInteger index){
			SQFloat f;
			sq_getfloat(vm, index, &f);
			return static_cast<T>(f);
		}
	};

	// Bool
	template <>
	struct X<bool>{
		static const char mask = 'b';
		static const char tag  = 'b';
		static void push(HSQUIRRELVM vm, bool val){ sq_pushbool(vm, val); }
		static bool get(HSQUIRRELVM vm, SQInteger index){
			SQBool b;
			sq_tobool(vm, index, &b);
			return (b != SQFalse);
		}
	};

	// C String
	template <>
	struct X<SQcstr>{
		static const char mask = 's';
		static const char tag  = 's';
		static void push(HSQUIRRELVM vm, SQcstr val){ sq_pushstring(vm, val, -1); }
		static SQcstr get(HSQUIRRELVM vm, SQInteger index){
			SQcstr s;
			sq_getstring(vm, index, &s);
			return s;
		}
	};

	// stl String
	template <>
	struct X<SQstring, false,false>{
		static const char mask = 's';
		static const char tag  = 's';
		static void push(HSQUIRRELVM vm, SQstring val){ sq_pushstring(vm, val.c_str(), val.length()); }
		static SQstring get(HSQUIRRELVM vm, SQInteger index){
			SQcstr s;
			sq_getstring(vm, index, &s);
			return SQstring(s);
		}
	};

	// CLASS
	template <class T>
	struct X<T,false,false>{
		static const char mask = 'x';
		static const char* tag;
		static HSQOBJECT obj;
		static void push_class(HSQUIRRELVM vm){
			if(sq_isclass(obj)){
				sq_pushobject(vm, obj);
			}else{
				sq_newclass(vm, SQFalse);
				sq_getstackobj(vm, -1, &obj);
				sq_addref(vm, &obj);
			}
		}
		static void push(HSQUIRRELVM vm, T& val){
			push_class(vm);
			sq_createinstance(vm, -1);
			sq_remove(vm, -2);
			sq_setinstanceup(vm, -1, &val);
		}
		static T& get(HSQUIRRELVM vm, SQInteger index){
			SQUserPointer p = nullptr;
			sq_getinstanceup(vm, index, &p, 0);
			return *static_cast<T*>(p);
		}
	};
	template <class T> const char* X<T, false, false>::tag = typeid(T).name();
	template <class T> HSQOBJECT X<T, false, false>::obj;

	// CLASS pointer
	template <class T>
	struct X<T*,false,false> : X<T,false,false>{
		static void push(HSQUIRRELVM vm, T* val){
			push_class(vm);
			sq_createinstance(vm, -1);
			sq_remove(vm, -2);
			sq_setinstanceup(vm, -1, val);
		}
		static T* get(HSQUIRRELVM vm, SQInteger index){
			SQUserPointer p = nullptr;
			sq_getinstanceup(vm, index, &p, 0);
			return static_cast<T*>(p);
		}
	};

//---------------------------------------------------------------------------
//	引数マスク
//---------------------------------------------------------------------------
	// 結合
	template <class... Types> struct TypeMask;
	template <> struct TypeMask<> {
		static SQstring mask(){ return SQstring(); }
		static SQstring tag(){  return SQstring(); }
	};
	template <class T, class... Types> struct TypeMask<T, Types...> {
		static SQstring mask(){ return X<std::decay<T>::type>::mask + TypeMask<Types...>::mask(); }
		static SQstring tag(){  return X<std::decay<T>::type>::tag  + TypeMask<Types...>::tag(); }
	};

	// 関数用マスク
	template <class... A> SQstring type_mask(){ return "." + TypeMask<A...>::mask(); }
	template <         class R, class... A> SQstring args_mask(R(   *f)(A...)     ){ return type_mask<A...>(); }
	template <class C, class R, class... A> SQstring args_mask(R(C::*f)(A...)     ){ return type_mask<A...>(); }
	template <class C, class R, class... A> SQstring args_mask(R(C::*f)(A...)const){ return type_mask<A...>(); }

	// オーバーロード用タグ
	template <class... A> SQstring type_tag(){ return TypeMask<A...>::tag(); }
	template <         class R, class... A> SQstring args_tag(R(   *f)(A...)     ){ return type_tag<A...>(); }
	template <class C, class R, class... A> SQstring args_tag(R(C::*f)(A...)     ){ return type_tag<A...>(); }
	template <class C, class R, class... A> SQstring args_tag(R(C::*f)(A...)const){ return type_tag<A...>(); }

	// 実行時引数タグ（オーバーロード用）
	SQstring args_code(HSQUIRRELVM vm){
		size_t size = sq_gettop(vm);
		SQsstream s;
		for (size_t i = 2; i <= size; ++i){
			switch (sq_gettype(vm, i)){
			case OT_INTEGER: s << 'i'; continue;
			case OT_FLOAT:   s << 'f'; continue;
			case OT_BOOL:    s << 'b'; continue;
			case OT_STRING:  s << 's'; continue;
			}
			SQUserPointer tag = 0;
			sq_gettypetag(vm, i, &tag);
			if(tag){ s << static_cast<SQcstr>(tag); }
		}
		return s.str();
	}

//------------------------------------------------------------------
//	スタブ
//------------------------------------------------------------------
	template <class F>
	struct Stub;

	// 終端（戻り値有無での分岐）
	template <>	struct Stub<void()> {
		template <class F> static int call(HSQUIRRELVM vm, SQInteger index, F f) { f(); return 0; }
	};
	template <class R> struct Stub<R()> {
		template <class F> static int call(HSQUIRRELVM vm, SQInteger index, F f) { X<R>::push(vm, f()); return 1; }
	};

	// 再起処理部分（引数取り出し）
	template <class R, class T, class... A>
	struct Stub<R(T, A...)> {
		template <class F>
		static int call(HSQUIRRELVM vm, SQInteger index, F f) {
			T val = X<std::decay<T>::type>::get(vm, index);
			return Stub<R(A...)>::call(vm, index + 1, [&](A ...a){ return f(val, a...); });
		}
	};

	// メンバー関数用始点
	template <class R, class C, class... A>
	struct Stub<R(C::*)(A...)> {
		template <class F>
		static int call(HSQUIRRELVM vm, SQInteger index, F f) {
			C* inst = X<C*>::get(vm, 1);
			return Stub<R(A...)>::call(vm, index, [&](A ...a){ return (inst->*f)(a...); });
		}
	};

	// 通常関数スタブ
	template <class F>
	SQInteger FunctionStub(HSQUIRRELVM vm)try{
		F f;
		sq_getuserpointer(vm, -1, reinterpret_cast<SQUserPointer*>(&f));
		return Stub<std::remove_pointer<F>::type>::call(vm, 2, *f);
	}catch (std::exception& e) {
		return sq_throwerror(vm, (string("error in function: ") + e.what()).c_str());
	}

	// メンバー関数スタブ
	template <class F>
	SQInteger MemberStub(HSQUIRRELVM vm)try{
		F *f;
		sq_getuserdata(vm, -1, reinterpret_cast<SQUserPointer*>(&f), nullptr);
		return Stub<F>::call(vm, 2, *f);
	}catch (std::exception& e){
		return sq_throwerror(vm, (string("error in member: ") + e.what()).c_str());
	}

	// コンストラクター用スタブ
	template <class C, class... A>
	SQInteger CtorStub(HSQUIRRELVM vm)try{
		return Stub<void(A...)>::call(vm, 2, [&](A ...a){
			sq_setinstanceup(vm, 1, new C(a...));
			sq_setreleasehook(vm, 1, [](SQUserPointer p, SQInteger)->int{ delete static_cast<C*>(p); return 0; });
		});
	}catch (std::exception& e) {
		return sq_throwerror(vm, (string("error in constructor: ") + e.what()).c_str());
	}

	// オーバーロード用スタブ
	SQInteger OverloadStub(HSQUIRRELVM vm){
		size_t size = sq_gettop(vm) - 1;
		sq_pushstring(vm, args_code(vm).c_str(), -1);
		if( SQ_FAILED(sq_get(vm, -2)) )return SQ_ERROR;
		for(size_t i=1; i<=size; ++i){ sq_push(vm, i); }
		sq_call(vm, size, SQTrue, SQTrue);
		return 1;
	}

//---------------------------------------------------------------------------
// テーブル
//---------------------------------------------------------------------------

	class Table{
	protected:
		HSQUIRRELVM vm;
		SQInteger self;

		Table(Table&& t):vm(t.vm), self(t.self){ t.self=0; }
		Table(const Table& t):vm(t.vm){ sq_push(vm, t.self); self=sq_gettop(vm); }
		Table& operator =(Table&&) = delete;
	    Table& operator =(Table const&) = delete;

		Table(HSQUIRRELVM vm, SQInteger self): vm(vm), self(self){}

	public:
		Table(HSQUIRRELVM vm): vm(vm), self(newtable(vm)){}
		~Table(){ if(self)sq_settop(vm, self-1); }

		// アクセス
		class Accessor{
			friend Table;
			HSQUIRRELVM vm;
			SQInteger self;
			SQInteger overload;

			Accessor(Accessor&& a): vm(a.vm), self(a.self), overload(a.overload){ a.overload=0; }
			Accessor(const Accessor&) = delete;
			Accessor& operator =(Accessor&&) = delete;
			Accessor& operator =(Accessor const&) = delete;

			Accessor(HSQUIRRELVM vm, SQInteger self, SQcstr name):
				vm(vm), self(self), overload(0){ sq_pushstring(vm, name, -1); }
		public:
			~Accessor(){
				if(overload){
					assert(sq_gettop(vm)==overload);
					sq_newclosure(vm, OverloadStub, 1);
					sq_newslot(vm, self, SQFalse);
				}
			}

			// バインド
			template <class T>
			const Accessor& operator <= (T t){
				ObjectTraits<T>::push(vm, t);
				sq_newslot(vm, self, SQFalse);
				return *this;
			}

			// オーバーロード
			template <class T>
			Accessor& operator << (T t){
				if(!overload)overload = newtable(vm);
				sq_pushstring(vm, args_tag(t).c_str(), -1);
				ObjectTraits<T>::push(vm, t);
				sq_newslot(vm, overload, SQFalse);
				return *this;
			}

			// 設定
			template <class T>
			const Accessor& operator = (const T& val){
				X<T>::push(vm, val);
				sq_set(vm, self);
				return *this;
			}

			// 取得
			template <class T>
			operator T(){
				if( SQ_FAILED(sq_get(vm, self)) )return T();
				T val = X<T>::get(vm, -1);
				sq_pop(vm, 1);
				return val;
			}
		};

		Table::Table(const Accessor& a) : vm(a.vm), self(0){
			if( SQ_FAILED(sq_get(vm, a.self)) )sq_newtable(vm);
			self = sq_gettop(vm);
		}

		Accessor operator[](SQcstr name){ return Accessor(vm, self, name); }

		static void push(HSQUIRRELVM vm, Table& t){
			sq_push(vm, t.self);
		}
	};

	// ルートテーブル
	struct RootTable:Table{
		RootTable(HSQUIRRELVM vm): Table(vm, (sq_pushroottable(vm), sq_gettop(vm))){}
	};
	// 定数テーブル
	struct ConstTable:Table{
		ConstTable(HSQUIRRELVM vm): Table(vm, (sq_pushconsttable(vm), sq_gettop(vm))){}
	};

	// クラス
	template <class C>
	class Class: public Table{
	public:
		Class(HSQUIRRELVM vm):
			Table(vm, (X<C>::push_class(vm), sq_gettop(vm))){}

		// コンストラクター定義
		template <class C>
		class Ctor{
			HSQUIRRELVM vm;
			SQInteger target;
			SQInteger self;

		public:
			Ctor(Ctor&&) = delete;
			Ctor(const Ctor&) = delete;
			Ctor& operator =(Ctor&&) = delete;
			Ctor& operator =(Ctor const&) = delete;

			Ctor(HSQUIRRELVM vm, SQInteger target):
				vm(vm), target(target), self(newtable(vm)){}

			~Ctor(){
				assert(sq_gettop(vm)==self);
				sq_newclosure(vm, OverloadStub, 1);
				sq_newslot(vm, target, SQFalse);
			}
			template <class... A>
			Ctor& New(){
				sq_pushstring(vm, type_tag<A...>().c_str(), -1);
				sq_newclosure(vm, CtorStub<C, A...>, 0);
				sq_newslot(vm, self, SQFalse);
				return *this;
			}
		};
		Ctor<C> operator()(){
			sq_pushstring(vm, "constructor", -1);
			return Ctor<C>(vm, self);
		}
	};

//---------------------------------------------------------------------------
//	バインド
//---------------------------------------------------------------------------
	// 関数
	template <class P>
	struct BindFunc{
		template <class F>
		static void push(HSQUIRRELVM vm, F f){
			auto p = static_cast<P>(f);
			sq_pushuserpointer(vm, p);
			sq_newclosure(vm, FunctionStub<P>, 1);
			sq_setparamscheck(vm, SQ_MATCHTYPEMASKSTRING, args_mask(p).c_str());
		}
	};

	// メンバー関数
	template <class P>
	struct BindMember{
		template <class F>
		static void push(HSQUIRRELVM vm, F f){
			memcpy(sq_newuserdata(vm, sizeof(f)), &f, sizeof(f));
			sq_newclosure(vm, MemberStub<P>, 1);
			sq_setparamscheck(vm, SQ_MATCHTYPEMASKSTRING, args_mask(f).c_str());
		}
	};

	// 処理の切り分け（関数）
	template <class F> struct FunctorTraits;
	template <class R, class C, class... A> struct FunctorTraits <R(C::*)(A...)>      :BindFunc<R(*)(A...)>{};
	template <class R, class C, class... A> struct FunctorTraits <R(C::*)(A...)const> :BindFunc<R(*)(A...)>{};
	template <class F> struct FunctionTraits : FunctorTraits <decltype(&F::operator())>{};
	template <class R, class... A> struct FunctionTraits <R(*)(A...)> : BindFunc<R(*)(A...)>{};
	template <class R, class C, class... A> struct FunctionTraits <R(C::*)(A...)>      :BindMember<R(C::*)(A...)>{};
	template <class R, class C, class... A> struct FunctionTraits <R(C::*)(A...)const> :BindMember<R(C::*)(A...)>{};
	template <class F> struct ObjectTraits : FunctionTraits<F>{};

	// ↓処理の切り分け（その他）

	// テーブル
	template <> struct ObjectTraits<Table> : Table{};
	template <> struct ObjectTraits<RootTable> : Table{};
	template <> struct ObjectTraits<ConstTable> : Table{};
	template <class C> struct ObjectTraits<Class<C>> : Table{};

	// 生のコールバック
	template <> struct ObjectTraits<SQFUNCTION>{
		static void push(HSQUIRRELVM vm, SQFUNCTION f){
			sq_newclosure(vm, f, 0);
		}
	};

//---------------------------------------------------------------------------
// 配列はひとまず保留
//---------------------------------------------------------------------------
/*
	struct Array{
		const HSQUIRRELVM vm;
		const SQInteger self;
		Array(HSQUIRRELVM vm, SQInteger self): vm(vm), self(self){}
		~Array(){ sq_settop(vm, self-1); }
		template <class T>
		void Append(const T& val){
			X<T>::push(vm, val);
			sq_arrayappend(vm, self);
		}
		void Pop(){ sq_arraypop(vm, self); }
		void Resize(SQInteger size){ sq_arrayresize(vm, self, size); }
		void Reverse(){ sq_arrayreverse(vm, self); }
	};
*/

}
	using detail::Table;
	using detail::Class;
	using detail::RootTable;
	using detail::ConstTable;
}
