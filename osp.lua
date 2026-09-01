local obs = require("obslua")
local bit = require("bit")
local ffi = require("ffi")

ffi.cdef[[
    unsigned long long init(void);
    void projector_patch_enable(void);
    void projector_patch_disable(void);
    void enable_only_fs_projector(void);
    void disable_only_fs_projector(void);
]]

local patch = ffi.load(script_path() .. "obs_stretched_projector")

local setting_enabled = false

local setting_ofsp_enabled = false

local full_compatible = obs.obs_get_version() == 0x20020002


function script_update(settings)

    local enabled = obs.obs_data_get_bool(settings, "ospEnabled")
    local enabled_ofsp = full_compatible and obs.obs_data_get_bool(settings, "ospOnlyFsProjector")

    print(enabled, enabled_ofsp)

    if enabled ~= setting_enabled then
        setting_enabled = enabled

        if enabled then
            patch.projector_patch_enable()
        else
            patch.projector_patch_disable()
        end
    end

    if enabled_ofsp ~= setting_ofsp_enabled then
        setting_ofsp_enabled = enabled_ofsp

        if enabled_ofsp then
            patch.enable_only_fs_projector()
        else
            patch.disable_only_fs_projector()
        end
    end

end


function script_unload()
    if setting_enabled then
        patch.projector_patch_disable()
        setting_enabled = false
    end
end


local function empty()
end

function script_load()
    local pBase = patch.init()
    if pBase == 0 then
        script_update, script_unload = empty, empty
    else
        print(string.format("Located pBase = 0x%x", pBase))
    end

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


    if full_compatible then

        obs.obs_properties_add_bool(
            props,
            "ospOnlyFsProjector",
            "Apply only on Fullscreen Projector"
        )

    end

    return props

end
