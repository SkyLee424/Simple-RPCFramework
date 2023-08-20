CXX = g++
TARGET = test
CXXFLAGS =  -std=c++17 -c -g
SRC = $(wildcard *.cpp) 
DEPEND = $(patsubst %.cpp, %.o, $(SRC))

$(TARGET): $(DEPEND)
	$(CXX) -o $@ $^ -pthread 

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ -pthread -I/home/skylee/Documents/WorkSpace/Demo/RPCFramework/NetworkLib

.PHONY: clean # clean 脚本
# .PHONY 是为了避免目录中还有一个叫 clean 的文件，导致提示 'clean is up to date'，clean 脚本不被执行
clean:
	rm -f $(TARGET)
	rm -f *.o