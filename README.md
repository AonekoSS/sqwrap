sqwrap（スクラップ）
======

ものぐさな人の為のSquirrel用バインダー  
ポロリ（オーバーロード機能）もあるよ。

制限とか
------
Squirrelのバージョンは3.0.7で調整しました。（たぶん2.2.5でも大丈夫？）
C++11対応で可変長引数テンプレートとか使える環境が必要です。
（Visual Studioだと2013以降くらい？）

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
	
		// C++側でオーバーロードされてる場合は明示的に取り出す必要あり
		root["FunctionO"]
			<< static_cast<void(*)(int)>(OverloadFunction)
			<< static_cast<void(*)(int,int)>(OverloadFunction);
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
		hoge().New<>().New<int>();

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

