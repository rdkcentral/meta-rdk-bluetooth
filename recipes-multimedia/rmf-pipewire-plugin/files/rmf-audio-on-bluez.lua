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

  local ok, ret = pcall(os.execute, cmd)
  if ok then
    log:info("systemctl executed successfully")
    return true
  end
  log:warning(string.format("os.execute failed: %s", tostring(ret)))
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

