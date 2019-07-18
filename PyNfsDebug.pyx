# cython: c_string_type=str, c_string_encoding=ascii
cdef extern from "NfsDebug.h":
  cdef cppclass c_NfsDebug "NfsDebug":
    c_NfsDebug(const char *, const char *, unsigned short) except+;
    void dumpMounts();

cdef class NfsDebug:
  cdef c_NfsDebug *nfsDbg_;

  def __cinit__(self, str host, str exp, int nfsLocPort=0):
    self.nfsDbg_ = new c_NfsDebug( host, exp, nfsLocPort )

  def dumpMounts(self):
    self.nfsDbg_.dumpMounts()

  def __dealloc__(self):
    del self.nfsDbg_
    
