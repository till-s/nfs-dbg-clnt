from libc.string cimport memcpy
from libc.stdint cimport uint32_t

cdef extern from "proto/nfs_prot.h":
  cdef struct nfs_fh:
    unsigned char data[32]
  cdef struct diropargs:
    nfs_fh dir
    char   * name

cdef extern from "NfsDebug.h":
  cdef cppclass c_FH "FH":
    c_FH(nfs_fh*)
    void set(diropargs*)
    nfs_fh *get()
  cdef cppclass c_NfsDebug "NfsDebug":
    c_NfsDebug(const char *, const char *, unsigned short) except+
    void     dumpMounts()
    int      lkup(diropargs *)
    int      read(nfs_fh*, unsigned int off, unsigned int count, void *buf);
    void     getFH(nfs_fh*)
    const nfs_fh* root()
    uint32_t getNfsXid()
    void     setNfsXid(uint32_t)
    void     rm(diropargs *)
    int      creat(diropargs *, nfs_fh *)

cdef class FH:
  cdef c_FH *fh_

  def __cinit__(self):
    fh_ = None

  def dealloc(self):
    del self.fh_

cdef object createFH(const nfs_fh *fh):
  rval = FH()
  rval.fh_ = new c_FH( fh )
  return rval


cdef class NfsDebug:
  cdef c_NfsDebug *nfsDbg_

  def __cinit__(self, str host, str exp, int nfsLocPort=0):
    cdef bytes hostb = host.encode('ascii')
    cdef bytes expb  = exp.encode('ascii')
    self.nfsDbg_ = new c_NfsDebug( hostb, expb, nfsLocPort )

  def __dealloc__(self):
    del self.nfsDbg_

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

  def getNfsXid(self):
    return self.nfsDbg_.getNfsXid()

  def setNfsXid(self, unsigned int xid):
    self.nfsDbg_.setNfsXid( xid )

  def rm(self, FH hdl, str name):
    cdef diropargs arg
    cdef bytes     b = name.encode('ascii')
    hdl.fh_.set( &arg )
    arg.name         = b
    self.nfsDbg_.rm( &arg )

  def creat(self, FH hdl, str name):
    cdef diropargs arg
    cdef nfs_fh    fh
    cdef bytes     b = name.encode('ascii')
    hdl.fh_.set( &arg )
    arg.name         = b
    if 0 != self.nfsDbg_.creat( &arg, &fh ):
      raise RuntimeError("Unable to create '{}'".format(name) )
    return createFH( &fh )
