all: attribute.o data_io.o model.o model_impl.o compression.o decompression.o sample

unit_test: attribute_test data_io_test model_impl_test

clean :
	rm *.o byte_writer_test.txt

attribute.o : attribute.cpp attribute.h base.h
	g++ -std=c++11 -Wall -c attribute.cpp

data_io.o : data_io.cpp data_io.h base.h
	g++ -std=c++11 -Wall -c data_io.cpp

model.o : model.cpp model_impl.h model.h
	g++ -std=c++11 -Wall -c model.cpp

model_impl.o : model_impl.cpp model_impl.h model.h
	g++ -std=c++11 -Wall -c model_impl.cpp

compression.o : compression.cpp compression.h model.h 
	g++ -std=c++11 -Wall -c compression.cpp

decompression.o : decompression.cpp decompression.h model.h
	g++ -std=c++11 -Wall -c decompression.cpp

sample : sample.cpp attribute.o data_io.o model.o model_impl.o compression.o decompression.o
	g++ -std=c++11 -Wall attribute.o data_io.o model.o model_impl.o compression.o decompression.o sample.cpp -o sample

attribute_exec : attribute.o attribute_test.cpp
	g++ -std=c++11 -Wall attribute.o attribute_test.cpp -o attribute_test

attribute_test : attribute_exec
	./attribute_test

data_io_exec : attribute.o data_io.o data_io_test.cpp
	g++ -std=c++11 -Wall attribute.o data_io.o data_io_test.cpp -o data_io_test

data_io_test : data_io_exec
	./data_io_test

model_impl_exec : model_impl.o attribute.o data_io.o model_impl_test.cpp
	g++ -std=c++11 -Wall model_impl.o attribute.o data_io.o model_impl_test.cpp -o model_impl_test

model_impl_test : model_impl_exec
	./model_impl_test

