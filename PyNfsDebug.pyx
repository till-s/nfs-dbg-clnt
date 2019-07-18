#

### cython: c_string_type=str, c_string_encoding=ascii

from cython.view cimport array as cvarray
from libc.string cimport memcpy

cdef extern from "proto/nfs_prot.h":
  ctypedef nfs_fh
  cdef struct p_nfs_fh "nfs_fh":
    unsigned char data[32]
  cdef struct diropargs:
    p_nfs_fh dir
    char   * name

cdef extern from "NfsDebug.h":
  cdef cppclass c_FH "FH":
    c_FH(p_nfs_fh*)
    void set(diropargs*)
    p_nfs_fh *get()
  cdef cppclass c_NfsDebug "NfsDebug":
    c_NfsDebug(const char *, const char *, unsigned short) except+
    void dumpMounts()
    int  lkup(diropargs *)
    int  read(p_nfs_fh*, unsigned int off, unsigned int count, void *buf);
    void getFH(p_nfs_fh*)
    const p_nfs_fh* root()

cdef class FH:
  cdef c_FH *fh_

  def __cinit__(self):
    fh_ = None

  def dealloc(self):
    del self.fh_

cdef object createFH(const p_nfs_fh *fh):
  rval = FH()
  rval.fh_ = new c_FH( fh )
  return rval


cdef class NfsDebug:
  cdef c_NfsDebug *nfsDbg_

  def __cinit__(self, str host, str exp, int nfsLocPort=0):
    cdef bytes hostb = host.encode('ascii')
    cdef bytes expb  = exp.encode('ascii')
    self.nfsDbg_ = new c_NfsDebug( hostb, expb, nfsLocPort )

  def dumpMounts(self):
    self.nfsDbg_.dumpMounts()

  def lkup(self, FH hdl, str path):
    cdef diropargs arg
    cdef bytes     b = path.encode('ascii')
    hdl.fh_.set( &arg )
    arg.name = b
    if 0 == self.nfsDbg_.lkup( &arg ):
      return createFH( & arg.dir )

  def root(self):
    return createFH( self.nfsDbg_.root() )

  def read(self, FH hdl, unsigned int off, unsigned int count):
    buf = bytearray(count)
    cdef char *bufp = buf
    if self.nfsDbg_.read( hdl.fh_.get(), off, count, bufp ) >= 0:
      return buf
    else:
      raise RuntimeError("Read Failure")

  def __dealloc__(self):
    del self.nfsDbg_
