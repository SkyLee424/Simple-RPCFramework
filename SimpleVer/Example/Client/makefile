CXX = clang++
TARGET = client
CXXFLAGS = -std=c++11 -c -g
INCLUDE_PATH = /home/skylee/Documents/WorkSpace/Demo/RPCFramework/SimpleVer/includes
SRC = $(wildcard *.cpp) # wildcard，用于防止通配符解析失败。使变量定义时，括号里的通配符仍然生效
DEPEND = $(patsubst %.cpp, %.o, $(SRC)) # patsubst，用于防止替换文件解析失效。替换文件后缀

$(TARGET): $(DEPEND)
	$(CXX) -o $@ $^ -pthread 

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ -pthread -I $(INCLUDE_PATH)

.PHONY: clean # clean 脚本
# .PHONY 是为了避免目录中还有一个叫 clean 的文件，导致提示 'clean is up to date'，clean 脚本不被执行
clean:
	rm -f $(TARGET)
	rm -f *.o