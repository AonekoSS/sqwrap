sqwrap（スクラップ）
======

ものぐさな人の為のSquirrel用バインダー  
ポロリ（オーバーロード機能）もあるよ。

制限とか
------
バインドできる関数およびメソッドの引数は8個までです。  
（C++11の可変長引数テンプレートに対応してないコンパイラとかでも大丈夫）


関数のバインド（オーバーロードの例）
------

これらの関数たちを……

	int Function();
	int FunctionInteger(int i);
	int FunctionString(const char* s);
	void OverloadFunction(int i);
	void OverloadFunction(int a, int b);
	
こんな感じでバインドできます。

	void bind(HSQUIRRELVM vm){
		sqwrap::RootTable root(vm);
	
		root["Function"] <= Function;
	
		// 引数の違う関数をひとつの名前にまとめてバインド
		root["FunctionV"] << FunctionInteger << FunctionString;
	
		// C++側でオーバーロードされてる場合はちょっと複雑
		root["FunctionO"]
			.Overload<void(*)(int)>(OverloadFunction)
			.Overload<void(*)(int,int)>(OverloadFunction);
	}


クラスのバインド
------
こんなクラスを……

	class Hoge{
	public:
		Hoge();
		Hoge(int);
		void Mathod();
		int MathodA(int);
		int MathodB(int, bool);
	}

こんな感じでバインドできます。

	void bind(HSQUIRRELVM vm){

		// クラスにコンストラクターを２種バインド
		sqwrap::Class<Hoge> hoge;
		hoge.Ctor().Ctor<int>();

		// 他は関数と同じ
		hoge["Mathod"] <= Hoge::Mathod;
		hoge["MathodV"] << Hoge::MathodA << Hoge::MathodB; 

		// メンバーを全部登録したらルートテーブルにえいっ！
		sqwrap::RootTable root(vm);
		root["Hoge"] <= hoge;
	}


変数へのアクセス（おまけ機能）
------
こんな感じで変数に値つっこめたりします。

	void put_x(HSQUIRRELVM vm, int val){
		sqwrap::RootTable root(vm);
		root["x"] = val;
	}


ライセンス
------
煮るなり焼くなりお好きにどうぞ。

