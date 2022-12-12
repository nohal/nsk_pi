## Building marnav for Windows

Download and install [CLang](https://github.com/llvm/llvm-project/releases/tag/llvmorg-15.0.6) for Windows
Download and install [Ninja](https://github.com/ninja-build/ninja/releases)

Patch marnav
```diff
diff --git a/src/marnav/nmea/io_double_strtodl.cpp b/src/marnav/nmea/io_double_strtodl.cpp
index 0e6795e..4af5730 100644
--- a/src/marnav/nmea/io_double_strtodl.cpp
+++ b/src/marnav/nmea/io_double_strtodl.cpp
@@ -19,10 +19,18 @@ void read(const std::string & s, double & value, data_format fmt)
        if (s.empty())
                return;

+#ifndef _WIN32
        static const locale_t locale = ::newlocale(LC_NUMERIC_MASK, "C", nullptr);
+#else
+       static const _locale_t locale = ::_create_locale(LC_ALL, "C");
+#endif

        char * endptr = nullptr;
+#ifndef _WIN32
        value = ::strtod_l(s.c_str(), &endptr, locale);
+#else
+       value = ::_strtod_l(s.c_str(), &endptr, locale);
+#endif
        if (endptr != s.c_str() + s.size())
                throw std::runtime_error{"invalid string to convert to double: [" + s + "]"};
 }
```


Build it
```sh
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang -DENABLE_TESTS=OFF -DENABLE_TESTS_BENCHMARK=OFF -DENABLE_TOOLS=OFF -DENABLE_EXAMPLES=OFF -G Ninja ..
ninja
```

Get the library from `src\marnav.lib`
