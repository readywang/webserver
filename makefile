CXX ?= g++

DEBUG ?= 1
ifeq ($(DEBUG), 1)
    CXXFLAGS += -g
else
    CXXFLAGS += -O2

endif

server: main.cpp ./cond/cond.h ./locker/locker.h ./thread_pool/thread_pool.h ./sem/sem.h ./timer/timer_list.cpp ./log/log.cpp  ./http_con/http_con.cpp ./sql/sql_connection_pool.cpp ./webserver/webserver.cpp ./config/config.cpp
	$(CXX) -o server $^ $(CXXFLAGS) -lpthread -L/usr/lib64/mysql -lmysqlclient

clean:
	rm -r server