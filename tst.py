import PyNfsDebug
c=PyNfsDebug.NfsDebug("127.0.0.1","/remote")
r=c.root()
print( c.read( c.lkup(r, "prog.sh" ), 100, 10 ) )
c.setNfsXid(0xdeadbeef)
print("XID {:x}".format( c.getNfsXid()) )
f = c.creat(c.root(), "nexist")
msg = bytearray("HELLO WORLD".encode("ascii"))
c.write( f, msg )
got = c.read( f, 100, 2 )
if got != msg[2:]:
  print("Readback failed; expected '{}', got '{}'".format(msg[2:], got))
else:
  c.rm(c.root(),"nexist")
