basler-objs = BaslerCamera.o BaslerInterface.o

SRCS = $(basler-objs:.o=.cpp) 

CXXFLAGS = -I../include -I../../../hardware/include -I../../../common/include \
			-I/opt/pylon/include -I/opt/pylon/include/genicam -DUSE_GIGE -Wall -pthread -fPIC -g \
			-I../../../../YAT/include 

all:	Basler.o

Basler.o:	$(basler-objs)
	$(LD) -o $@ -r $+

#FrelonBase.o:	Basler.cpp
#	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f *.o *.P

%.o : %.cpp
	$(COMPILE.cpp) -MD $(CXXFLAGS) -o $@ $<
	@cp $*.d $*.P; \
	sed -e 's/#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$//' \
	-e '/^$$/ d' -e 's/$$/ :/' < $*.d >> $*.P; \
	rm -f $*.d

-include $(SRCS:.cpp=.P)
