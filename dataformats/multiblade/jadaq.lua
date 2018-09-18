
-- Copyright (C) 2018 European Spallation Source ERIC
-- Wireshark plugin for dissecting JADAQ readout data

-- helper variable and functions

-- jadaq header from: https://github.com/ess-dmsc/jadaq/blob/devel/DataFormat.hpp


-- -----------------------------------------------------------------------------------------------
-- the protocol dissector
-- -----------------------------------------------------------------------------------------------
jadaq_proto = Proto("jadaq","JADAQ Protocol")

function jadaq_proto.dissector(buffer,pinfo,tree)
	pinfo.cols.protocol = "JADAQ"
	local protolen = buffer():len()

	local data_length_byte = 8

	if protolen >= 32 then
    local run_lo   = buffer( 0, 4):le_uint()
    local run_hi   = buffer( 4, 4):le_uint()
    local time_lo   = buffer( 8, 4):le_uint()
    local time_hi   = buffer(12, 4):le_uint()
    local digit = buffer(16, 4):le_uint()
    local elemid = buffer(20, 2):le_uint()
    local hits = buffer(22, 2):le_uint()
    local vermaj = buffer(24, 1):le_uint()
    local vermin = buffer(25, 1):le_uint()

    local jadaqhdr = tree:add(jadaq_proto,buffer(),
       string.format("JADAQ digitizer: %d, hits: %d", digit, hits))

    jadaqhdr:add(buffer( 0, 8),
                 string.format("runid: %08x%08x", run_hi, run_lo))
    jadaqhdr:add(buffer( 8, 8),
                 string.format("time: %08x%08x", time_hi, time_lo))
    jadaqhdr:add(buffer(16, 4), "digitizer " .. digit)
    jadaqhdr:add(buffer(20, 2), "element id " .. elemid)
    jadaqhdr:add(buffer(22, 2), "number of elements " .. hits)
    jadaqhdr:add(buffer(24, 2), "version " .. vermaj .. "." .. vermin)

		pinfo.cols.info = string.format("digitizer: %3d, hits: %3d", digit, hits)



		for i=1,hits do
      local offset = 32 + (i - 1) * data_length_byte
      local time =    buffer(offset    , 4):le_uint()
      local channel = buffer(offset + 4, 2):le_uint()
      local adc =     buffer(offset + 6, 2):le_uint()
      jadaqhdr:add(buffer(offset, data_length_byte),
                   string.format("time %10d, channel %3d, adc %5d", time, channel, adc))
    end
	end
end

-- Register the protocol
udp_table = DissectorTable.get("udp.port")
udp_table:add(9000, jadaq_proto)
