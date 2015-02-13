all: attribute.o data_io.o utility.o model.o categorical_model.o numerical_model.o string_model.o compression.o decompression.o sample

unit_test: attribute_test data_io_test utility_test categorical_model_test model_test compression_test numerical_model_test string_model_test

clean :
	rm *.o byte_writer_test.txt compression_test.txt

attribute.o : attribute.cpp attribute.h base.h
	g++ -std=c++11 -Wall -c attribute.cpp

data_io.o : data_io.cpp data_io.h base.h
	g++ -std=c++11 -Wall -c data_io.cpp

utility.o : utility.cpp utility.h base.h
	g++ -std=c++11 -Wall -c utility.cpp

model.o : model.cpp model.h base.h attribute.h categorical_model.h numerical_model.h string_model.h
	g++ -std=c++11 -Wall -c model.cpp

categorical_model.o : categorical_model.cpp categorical_model.h attribute.h base.h model.h utility.h
	g++ -std=c++11 -Wall -c categorical_model.cpp

numerical_model.o : numerical_model.cpp numerical_model.h attribute.h base.h model.h utility.h
	g++ -std=c++11 -Wall -c numerical_model.cpp

string_model.o : string_model.cpp string_model.h attribute.h base.h model.h
	g++ -std=c++11 -Wall -c string_model.cpp

compression.o : compression.cpp compression.h model.h base.h
	g++ -std=c++11 -Wall -c compression.cpp

decompression.o : decompression.cpp decompression.h model.h
	g++ -std=c++11 -Wall -c decompression.cpp

sample : sample.cpp attribute.o data_io.o model.o categorical_model.o numerical_model.o string_model.o compression.o decompression.o utility.o
	g++ -std=c++11 -Wall attribute.o data_io.o model.o categorical_model.o numerical_model.o string_model.o compression.o decompression.o utility.o sample.cpp -o sample

attribute_exec : attribute.o attribute_test.cpp
	g++ -std=c++11 -Wall attribute.o attribute_test.cpp -o attribute_test

attribute_test : attribute_exec
	./attribute_test

data_io_exec : attribute.o data_io.o data_io_test.cpp
	g++ -std=c++11 -Wall attribute.o data_io.o data_io_test.cpp -o data_io_test

data_io_test : data_io_exec
	./data_io_test

utility_exec : utility.o utility_test.cpp
	g++ -std=c++11 -Wall utility.o utility_test.cpp -o utility_test

utility_test : utility_exec
	./utility_test

categorical_model_exec : categorical_model.o attribute.o data_io.o utility.o categorical_model_test.cpp
	g++ -std=c++11 -Wall categorical_model.o attribute.o data_io.o utility.o categorical_model_test.cpp -o categorical_model_test

categorical_model_test : categorical_model_exec
	./categorical_model_test

numerical_model_exec : numerical_model.o attribute.o data_io.o utility.o numerical_model_test.cpp
	g++ -std=c++11 -Wall numerical_model.o attribute.o data_io.o utility.o numerical_model_test.cpp -o numerical_model_test

numerical_model_test : numerical_model_exec
	./numerical_model_test

string_model_exec : string_model.o attribute.o data_io.o utility.o string_model_test.cpp
	g++ -std=c++11 -Wall string_model.o attribute.o data_io.o utility.o string_model_test.cpp -o string_model_test

string_model_test : string_model_exec
	./string_model_test

model_exec : model.o categorical_model.o numerical_model.o string_model.o attribute.o data_io.o utility.o model_test.cpp
	g++ -std=c++11 -Wall model.o categorical_model.o numerical_model.o string_model.o attribute.o data_io.o utility.o model_test.cpp -o model_test

model_test : model_exec
	./model_test

compression_exec : model.o categorical_model.o numerical_model.o string_model.o attribute.o data_io.o utility.o compression.o compression_test.cpp
	g++ -std=c++11 -Wall model.o categorical_model.o numerical_model.o string_model.o attribute.o data_io.o utility.o compression.o compression_test.cpp -o compression_test

compression_test : compression_exec
	./compression_test

