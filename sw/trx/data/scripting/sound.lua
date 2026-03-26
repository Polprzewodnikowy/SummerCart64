local raw = trxc.sound

local sound = {}

function sound.is_available(id)
  return raw.is_available(id)
end

function sound.play(id, opts)
  raw.play(id, opts)
end

function sound.stop(id)
  raw.stop(id)
end

function sound.stop_all()
  raw.stop_all()
end

trx.sound = sound
