local obs = obslua
local ffi = require("ffi")

ffi.cdef[[
    unsigned long long init(void);
    void projector_patch_enable(void);
    void projector_patch_disable(void);
]]

ffi.load(script_path() .. "obs_stretched_projector")

local setting_enabled = false

function script_load()
    local pBase = patch.init()
    print(string.format("Located pBase = 0x%x", pBase))
end

function script_description()
    return [[
<h2>Projector Stretch</h2>
<p>
Stretch OBS Projectors to fill the entire output area,
regardless of the canvas aspect ratio.
</p>
]]
end


function script_properties()

    local props = obs.obs_properties_create()

    obs.obs_properties_add_bool(
        props,
        "ospEnabled",
        "Enable Projector Stretching"
    )

    return props

end


function script_update(settings)

    local enabled = obs.obs_data_get_bool(settings, "ospEnabled")

    if enabled ~= setting_enabled then
        setting_enabled = enabled

        if enabled then
            patch.projector_patch_enable()
            print("[Projector Stretch] Enabled")
        else
            patch.projector_patch_disable()
            print("[Projector Stretch] Disabled")
        end
    end

end


function script_unload()
    if setting_enabled then
        patch.projector_patch_disable()
        setting_enabled = false
    end
end