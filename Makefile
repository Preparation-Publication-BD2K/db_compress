all: attribute.o data_io.o model.o model_impl.o compression.o decompression.o sample

unit_test: attribute_test

clean :
	rm *.o

attribute.o : attribute.cpp attribute.h base.h
	g++ -std=c++11 -Wall -c attribute.cpp

data_io.o : data_io.cpp data_io.h base.h
	g++ -Wall -c data_io.cpp

model.o : model.cpp model_impl.h model.h
	g++ -std=c++11 -Wall -c model.cpp

model_impl.o : model_impl.cpp model_impl.h model.h
	g++ -std=c++11 -Wall -c model_impl.cpp

compression.o : compression.cpp compression.h model.h 
	g++ -std=c++11 -Wall -c compression.cpp

decompression.o : decompression.cpp decompression.h model.h
	g++ -Wall -c decompression.cpp

sample : sample.cpp attribute.o data_io.o model.o model_impl.o compression.o decompression.o
	g++ -Wall attribute.o data_io.o model.o model_impl.o compression.o decompression.o sample.cpp -o sample

attribute_exec : attribute.o attribute_test.cpp
	g++ -std=c++11 -Wall attribute.o attribute_test.cpp -o attribute_test

attribute_test : attribute_exec
	./attribute_test

