
# 指令编译器和选项
CC=g++
CFLAGS=-pthread
 
# 目标文件
TARGET=server
SRCS = server.cpp \

 #头文件
INC = -I./EasyTcpServer -I./MessageHeader
 
OBJS = $(SRCS:.cpp=.o)

#注意由于引入了线程库 因此生成.o文件和链接文件 都需要添加线程头文件
$(TARGET):$(OBJS)
#	@echo TARGET:$@
#	@echo OBJECTS:$^
	$(CC) -o $@ $^ $(CFLAGS)
 
clean:
	rm -rf $(TARGET) $(OBJS)
 
#注意由于引入了线程库 因此生成.o文件和链接文件 都需要添加线程头文件
%.o:%.cpp
	$(CC)  $(INC) -o $@ -c $< $(CFLAGS)
