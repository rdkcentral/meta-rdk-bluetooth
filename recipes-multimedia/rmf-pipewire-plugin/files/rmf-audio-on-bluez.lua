-- Start/stop the RMF audio capture source (via a systemd unit) based on
-- BlueZ device connect/disconnect events.

log = Log.open_topic("s-rmf-audio-bluez")

log:info("Executing the lua script to spawn the rmf-audio-capture-pipewire")

local RMF_UNIT = "rmf-audio-capture-pipewire.service"

-- Track connected bluez devices by bound-id
local bluez_devices = {}
local rmf_active = false

-- Run systemctl without blocking WirePlumber's event loop.
local function systemctl(verb)
  local cmd = string.format("systemctl --no-block %s %s", verb, RMF_UNIT)
  log:info("Running: " .. cmd)

  local ok, r1, r2, r3 = pcall(os.execute, cmd)
  if not ok then
    log:warning(string.format("os.execute failed: %s", tostring(r1)))
    return false
  end

  -- Normalize the exit status across Lua versions. Only a genuine exit
  -- code of 0 counts as success; a non-zero code or termination by signal
  -- is a failure, so rmf_active stays in sync with the real unit state.
  --  * 5.1:   os.execute returns a numeric status (0 == success)
  --  * 5.2+:  os.execute returns (true|nil), "exit"|"signal", code
  local exit_code
  if type(r1) == "number" then
    exit_code = r1            -- Lua 5.1
  elseif r1 == true then
    exit_code = 0             -- Lua 5.2+ success
  elseif r2 == "exit" then
    exit_code = r3            -- Lua 5.2+ non-zero exit
  else
    exit_code = -1            -- Lua 5.2+ killed by signal (or unknown)
  end

  if exit_code == 0 then
    log:info("systemctl executed successfully")
    return true
  end

  log:warning(string.format("systemctl returned non-zero status: %s %s %s", tostring(r1), tostring(r2), tostring(r3)))
  return false
end

local function start_rmf_audio_source()
  if rmf_active then
    log:info("RMF audio source already requested")
    return
  end
  log:info("Starting RMF audio capture source")
  if systemctl("start") then
    rmf_active = true
  end
end

local function stop_rmf_audio_source()
  if not rmf_active then
    log:info("RMF audio source not running")
    return
  end
  log:info("Stopping RMF audio capture source")
  if systemctl("stop") then
    rmf_active = false
  end
end

-- Use global (not local) to prevent Lua GC from collecting the ObjectManager
-- wrapper and disconnecting signal handlers after script chunk finishes.
om = ObjectManager {
  Interest {
    type = "device",
    Constraint { "device.api", "equals", "bluez5" },
  }
}

-- DEBUG: track ObjectManager lifecycle
om:connect("installed", function (om)
  log:info("ObjectManager installed (initial scan complete)")
  local count = om:get_n_objects()
  log:info(string.format("ObjectManager currently tracking %d objects", count))
end)

om:connect("object-added", function (om, device)
  local device_id = device["bound-id"]
  local device_name = device.properties["device.name"] or "unknown"
  local device_desc = device.properties["device.description"] or device_name

  log:info(string.format("Bluez device connected: %s (ID: %d)", device_desc, device_id))
  bluez_devices[device_id] = { name = device_name, description = device_desc }

  -- First bluez device -> start the source
  start_rmf_audio_source()
end)

om:connect("object-removed", function (om, device)
  local device_id = device["bound-id"]
  local info = bluez_devices[device_id]
  if info then
    log:info(string.format("Bluez device disconnected: %s (ID: %d)",
                           info.description, device_id))
    bluez_devices[device_id] = nil
  end

  -- Last bluez device gone -> stop the source
  if next(bluez_devices) == nil then
    stop_rmf_audio_source()
  end
end)

om:activate()
log:info("RMF Audio Bluez monitor loaded - ObjectManager activated")

