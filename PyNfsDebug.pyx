from libc.string cimport memcpy
from libc.stdint cimport uint32_t

cdef extern from "proto/nfs_prot.h":
  cdef struct nfs_fh:
    unsigned char data[32]
  cdef struct nfscookie:
    unsigned char data[4]
  cdef struct diropargs:
    nfs_fh dir
    char   * name
  cdef struct entry:
    unsigned int fileid
    char        *name
    nfscookie    cookie
    entry       *nextentry

cdef extern from "NfsDebug.h":
  cdef cppclass c_FH "FH":
    c_FH(nfs_fh*)
    void set(diropargs*)
    nfs_fh *get()
  cdef cppclass c_DH "DH":
    c_DH(nfscookie *)
    c_DH()
    nfscookie *get()
  cdef cppclass c_NfsDebug "NfsDebug":
    c_NfsDebug(const char *srv, const char *mnt, const char *nfscred, unsigned short locNfsPort, const char *mntcred, unsigned short locMntPort) except+;
    c_NfsDebug(const char *srv, nfs_fh *mnt, const char *nfscred, unsigned short locNfsPort) except+;
    void     dumpMounts()
    int      lkup(diropargs *)
    int      read (nfs_fh*, unsigned int off, unsigned int count, void *buf);
    int      write(nfs_fh*, unsigned int off, unsigned int count, void *buf);
    void     getFH(nfs_fh*)
    const nfs_fh* root()
    uint32_t getNfsXid()
    void     setNfsXid(uint32_t)
    void     rm(diropargs *)
    int      creat(diropargs *, nfs_fh *)
    entry   *ls(nfs_fh *, unsigned count, nfscookie *cp)
  cdef void FreeEntry(entry*)

cdef class FH:
  cdef c_FH *fh_

  def __cinit__(self, bytearray a = None):
    if None == a:
      self.fh_ = <c_FH*>0
    else:
      self.fh_ = createFHFromByteArray( a )

  def __iter__(self):
    if self.fh_:
      i = 0
      while i < 32:
        yield self.fh_.get().data[i]
        i = i + 1

  def dump(self):
    if self.fh_:
      for i in range(32):
        print(self.fh_.get().data[i])
    else:
      print("FH is NULL")

  def dealloc(self):
    del self.fh_

cdef object createFH(const nfs_fh *fh):
  rval = FH()
  rval.fh_ = new c_FH( fh )
  return rval

cdef c_FH *createFHFromByteArray(bytearray a):
  cdef nfs_fh x
  if len(a) != 32:
    raise RuntimeError( "FH can only be created from bytearray of length {:d}".format( 32 ) )
  for i in range( len(a) ):
    x.data[i] = a[i]
  return new c_FH( &x )

cdef class NfsDebug:
  cdef c_NfsDebug *nfsDbg_

  def __cinit__(self, str host, object exp, int nfsLocPort=0):
    cdef bytes hostb = host.encode('ascii')
    cdef bytes expb
    cdef FH    expf
    if isinstance( exp, FH ):
      expf = exp
      self.nfsDbg_ = new c_NfsDebug( hostb, expf.fh_.get(), NULL, nfsLocPort )
    elif isinstance( exp, str ):
      expb = exp.encode('ascii')
      self.nfsDbg_ = new c_NfsDebug( hostb, expb, NULL, nfsLocPort, NULL, 0 )
    else:
      raise RuntimeError("Unable to create a NfsDebug from this type of object")

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

  def read(self, FH hdl, unsigned int count, unsigned int off = 0):
    buf = bytearray(count)
    cdef char *bufp = buf
    got = self.nfsDbg_.read( hdl.fh_.get(), off, count, bufp )
    if got >= 0:
      del buf[got:]
      return buf
    else:
      raise RuntimeError("Read Failure")

  def write(self, FH hdl, bytearray buf, unsigned int off = 0):
    cdef char *bufp = buf
    put = self.nfsDbg_.write( hdl.fh_.get(), off, len(buf), bufp )
    if put < 0:
      raise RuntimeError("Write Failure")
    return put


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

  def ls(self, FH hdl):
    cdef diropargs arg
    cdef entry    *res
    cdef entry    *ep
    cdef entry    *pp
    cdef c_DH      dh
    l = []
    hdl.fh_.set( &arg )
    while True:
      res = self.nfsDbg_.ls( &arg.dir, 1024, dh.get() )
      if not res:
        return l
      ep  = res
      # since res is non-NULL, ep is non-NULL here
      # and pp is always set (and non-NULL)
      while ep:
        pp = ep
        l.append( ep.name.decode('ascii') )
        ep = pp.nextentry
      dh = c_DH( &pp.cookie )
      FreeEntry( res )

