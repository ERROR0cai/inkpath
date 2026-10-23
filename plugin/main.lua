-- Inkpath: route transcribed file/clipboard strokes to the next page.
-- Windows clipboard helper: clipboard_to_png.ps1 in this directory.
-- Callbacks are global because Xournal++ resolves callbacks by name.

local nativeInitialized = false

local function pluginDirectory()
    local source = debug.getinfo(1, "S").source
    if source:sub(1, 1) == "@" then source = source:sub(2) end
    return source:match("^(.*[\\/])") or ""
end

local function showMessage(text)
    app.openDialog(text, {"OK"}, "", false)
end


-- Reload output preferences on every import. C++ independently reads the
-- same INI for its image-processing parameters.
local function loadOutputConfig()
    local result = {scale=0.5, offset_x=15, offset_y=15,
                    min_stroke_points=2, next_page=1, polarity_mode=0}
    local file = io.open(pluginDirectory() .. "config.ini", "rb")
    if not file then return result end
    local section = ""
    for rawLine in file:lines() do
        local line = rawLine
        line = line:gsub("^\239\187\191", "")
        line = line:gsub("[;#].*$", "")
        line = line:match("^%s*(.-)%s*$")
        local header = line:match("^%[([^%]]+)%]$")
        if header then section = header end
        if section == "output" or
           (section == "image" and line:match("^polarity_mode%s*=")) then
            local key, value = line:match("^([%w_]+)%s*=%s*(.-)%s*$")
            if key and result[key] ~= nil then
                local n = tonumber(value)
                if not n or n ~= n or n == math.huge or n == -math.huge then
                    file:close(); error("Inkpath config.ini: invalid " .. key, 0)
                end
                if key == "scale" and (n <= 0 or n > 10) or
                   (key == "offset_x" or key == "offset_y") and (n < -10000 or n > 10000) or
                   key == "min_stroke_points" and (n < 2 or n > 10000 or n % 1 ~= 0) or
                   key == "next_page" and (n ~= 0 and n ~= 1) or
                   key == "polarity_mode" and (n % 1 ~= 0 or n < 0 or n > 2) then
                    file:close(); error("Inkpath config.ini: out of range " .. key, 0)
                end
                result[key] = n
            end
        end
    end
    file:close()
    return result
end

local function ensureNative()
    if nativeInitialized then return end
    local windows = package.config:sub(1, 1) == "\\"
    local lib = windows and (pluginDirectory() .. "libinkpath.dll")
                        or "/usr/share/xournalpp/plugins/ImageTranscription/libinkpath.so"
    local load, err = package.loadlib(lib, "luaopen_loadInkpath")
    if not load then error("Inkpath DLL load failed: " .. tostring(err), 0) end
    load()
    nativeInitialized = true
end

-- Route imported strokes to the page immediately after the original current page.
-- The caller prepares strokes first, so empty/invalid images do not create a page.
local function targetNextPage()
    local doc = app.getDocumentStructure()
    if type(doc) ~= "table" or type(doc.pages) ~= "table" or
       type(doc.currentPage) ~= "number" then
        error("Inkpath: cannot determine current page and page count", 0)
    end
    local current = doc.currentPage
    local count = #doc.pages
    if current < 1 or current > count then
        error("Inkpath: current page index is invalid", 0)
    end
    local target = current + 1
    if target > count then
        -- Create a page directly after the current page.
        app.setCurrentPage(current)
        app.uiAction({ action = "ACTION_NEW_PAGE_AFTER" })
        local updated = app.getDocumentStructure()
        if type(updated) ~= "table" or type(updated.pages) ~= "table" or
           #updated.pages < target then
            error("Inkpath: could not create the next page", 0)
        end
    end
    app.setCurrentPage(target)
    return target
end

local function transcribeImage(path, mode)
    ensureNative()
    local cfg = loadOutputConfig()
    local selectedMode = mode ~= nil and mode or cfg.polarity_mode
    local obj = Inkpath(path, selectedMode, pluginDirectory() .. "config.ini")
    -- 原生模块根据 config.ini 中的开关返回诊断内容；关闭时为空。
    if type(obj.getDiagnosticReport) == "function" then
        local report = obj:getDiagnosticReport()
        if report and report ~= "" then showMessage(report) end
    end
    local count = obj:getStrokeCount()
    local strokes = {}
    for i = 0, count - 1 do
        if obj:getStrokeLength(i) >= cfg.min_stroke_points then
            local xs, ys = obj:getStroke(i, cfg.scale)

            -- 页面左边距和上边距
            for j = 1, #xs do
                xs[j] = xs[j] + cfg.offset_x
                ys[j] = ys[j] + cfg.offset_y
            end

            strokes[#strokes + 1] = {
                x = xs,
                y = ys,
                tool = "pen"
            }
        end
    end

    if #strokes == 0 then
        showMessage("Inkpath: no valid strokes detected in the image.")
        return
    end

    -- Only change/create pages after successful image vectorization.
    local destination = cfg.next_page == 1 and targetNextPage() or app.getDocumentStructure().currentPage

    app.addStrokes({
        strokes = strokes,
        allowUndoRedoAction = "grouped"
    })

    app.refreshPage()

    if type(app.scrollToPage) == "function" then
        pcall(app.scrollToPage, destination)
    end

    print("Inkpath: added " .. tostring(#strokes) ..
          " strokes to page " .. tostring(destination))
end

function inkpathFromFileDarkOnLight()
    local path = app.getFilePath({"*.jpg", "*.jpeg", "*.png", "*.bmp"})
    if path and path ~= "" then transcribeImage(path, 1) end
end

function inkpathFromFileLightOnDark()
    local path = app.getFilePath({"*.jpg", "*.jpeg", "*.png", "*.bmp"})
    if path and path ~= "" then transcribeImage(path, 2) end
end

function inkpathFromFile()
    local path = app.getFilePath({"*.jpg", "*.jpeg", "*.png", "*.bmp"})
    if path and path ~= "" then transcribeImage(path) end
end

function inkpathFromClipboard()
    if package.config:sub(1, 1) ~= "\\" then
        showMessage("Inkpath clipboard support is Windows-only.")
        return
    end
    local helper = pluginDirectory() .. "clipboard_to_png.ps1"
    local f = io.open(helper, "rb")
    if not f then
        showMessage("Inkpath: clipboard_to_png.ps1 not found in the plugin folder.")
        return
    end
    f:close()
    local tmp = os.getenv("TEMP") or os.getenv("TMP")
    if not tmp or tmp == "" then
        showMessage("Inkpath: Windows TEMP directory not found.")
        return
    end
    local imageFile = tmp .. "\\inkpath_clipboard_" .. os.time() .. "_" .. math.random(100000, 999999) .. ".png"
    local cmd = 'powershell.exe -NoProfile -NonInteractive -STA -ExecutionPolicy Bypass -WindowStyle Hidden -File "' ..
                helper .. '" -OutputPath "' .. imageFile .. '"'
    local ok, why, code = os.execute(cmd)
    local check = io.open(imageFile, "rb")
    if check then check:close() end
    if not check or ok ~= true then
        os.remove(imageFile)
        showMessage("Inkpath: clipboard does not contain an image, or export failed. Code: " .. tostring(code or why or ok))
        return
    end
    local worked, result = pcall(transcribeImage, imageFile)
    os.remove(imageFile)
    if not worked then error(result, 0) end
end

function initUi()
    -- Use direct menu entries to make registration visible and testable.
    -- Each accelerator belongs to the actual menu action.
    app.registerUi({
        menu = "Inkpath - From image file",
        callback = "inkpathFromFile",
        accelerator = "<Control><Alt>t"
    })
    app.registerUi({
        menu = "Inkpath - Force white background / black strokes",
        callback = "inkpathFromFileDarkOnLight"
    })
    app.registerUi({
        menu = "Inkpath - Force black background / white strokes",
        callback = "inkpathFromFileLightOnDark"
    })
    app.registerUi({
        menu = "Inkpath - From clipboard image",
        callback = "inkpathFromClipboard",
        accelerator = "<Control><Alt>g"
    })
end
