local raw = trxc.music

local music = {
  PlayMode = raw.PlayMode,
}

function music.get_track()
  return raw.get_track()
end

function music.play(id, opts)
  opts = opts or {}
  mode = opts.mode or trx.music.PlayMode.ONCE
  raw.play(id, mode)
end

function music.pause()
  raw.pause()
end

function music.unpause()
  raw.unpause()
end

function music.stop()
  raw.stop()
end

music.play_track = music.play

trx.music = music
