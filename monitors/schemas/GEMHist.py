# automatically generated by the FlatBuffers compiler, do not modify

# namespace: 

import flatbuffers

class GEMHist(object):
    __slots__ = ['_tab']

    @classmethod
    def GetRootAsGEMHist(cls, buf, offset):
        n = flatbuffers.encode.Get(flatbuffers.packer.uoffset, buf, offset)
        x = GEMHist()
        x.Init(buf, n + offset)
        return x

    # GEMHist
    def Init(self, buf, pos):
        self._tab = flatbuffers.table.Table(buf, pos)

    # GEMHist
    def Xhist(self, j):
        o = flatbuffers.number_types.UOffsetTFlags.py_type(self._tab.Offset(4))
        if o != 0:
            a = self._tab.Vector(o)
            return self._tab.Get(flatbuffers.number_types.Uint32Flags, a + flatbuffers.number_types.UOffsetTFlags.py_type(j * 4))
        return 0

    # GEMHist
    def Xhist_as_numpy_array(self):
        import numpy
        o = flatbuffers.number_types.UOffsetTFlags.py_type(self._tab.Offset(4))
        if o != 0:
            return numpy.frombuffer(
                self._tab.Bytes,
                numpy.dtype('<u4'),
                self._tab.VectorLen(o),
                self._tab.Vector(o)
            )
        return None

    # GEMHist
    def XhistLength(self):
        o = flatbuffers.number_types.UOffsetTFlags.py_type(self._tab.Offset(4))
        if o != 0:
            return self._tab.VectorLen(o)
        return 0

    # GEMHist
    def Yhist(self, j):
        o = flatbuffers.number_types.UOffsetTFlags.py_type(self._tab.Offset(6))
        if o != 0:
            a = self._tab.Vector(o)
            return self._tab.Get(flatbuffers.number_types.Uint32Flags, a + flatbuffers.number_types.UOffsetTFlags.py_type(j * 4))
        return 0

    # GEMHist
    def Yhist_as_numpy_array(self):
        import numpy
        o = flatbuffers.number_types.UOffsetTFlags.py_type(self._tab.Offset(6))
        if o != 0:
            return numpy.frombuffer(
                self._tab.Bytes,
                numpy.dtype('<u4'),
                self._tab.VectorLen(o),
                self._tab.Vector(o)
            )
        return None

    # GEMHist
    def YhistLength(self):
        o = flatbuffers.number_types.UOffsetTFlags.py_type(self._tab.Offset(6))
        if o != 0:
            return self._tab.VectorLen(o)
        return 0

def GEMHistStart(builder): builder.StartObject(2)
def GEMHistAddXhist(builder, xhist): builder.PrependUOffsetTRelativeSlot(0, flatbuffers.number_types.UOffsetTFlags.py_type(xhist), 0)
def GEMHistStartXhistVector(builder, numElems): return builder.StartVector(4, numElems, 4)
def GEMHistAddYhist(builder, yhist): builder.PrependUOffsetTRelativeSlot(1, flatbuffers.number_types.UOffsetTFlags.py_type(yhist), 0)
def GEMHistStartYhistVector(builder, numElems): return builder.StartVector(4, numElems, 4)
def GEMHistEnd(builder): return builder.EndObject()