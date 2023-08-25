CXX = clang++
TARGET = client
CXXFLAGS =  -std=c++17 -c -g
INCLUDE_PATH = -I/home/skylee/Documents/WorkSpace/Demo/RPCFramework/RPCFramework/includes # 这里替换为你自己实际的路径
LibFLAGS = -lpthread
SRC = $(wildcard *.cpp) 
DEPEND = $(patsubst %.cpp, %.o, $(SRC))

$(TARGET): $(DEPEND)
	$(CXX) -o $@ $^ $(LibFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ $(INCLUDE_PATH)

.PHONY: clean # clean 脚本
# .PHONY 是为了避免目录中还有一个叫 clean 的文件，导致提示 'clean is up to date'，clean 脚本不被执行
clean:
	rm -f $(TARGET)
	rm -f *.o
