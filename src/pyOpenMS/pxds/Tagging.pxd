from Types cimport *

cdef extern from "<OpenMS/METADATA/Tagging.h>" namespace "OpenMS":
    
    cdef cppclass Tagging:

        Tagging() nogil except + 
        Tagging(Tagging) nogil except + 

        double getMassShift() nogil except +
        void setMassShift(double mass_shift) nogil except +


