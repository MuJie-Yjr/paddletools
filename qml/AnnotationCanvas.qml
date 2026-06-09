import QtQuick

Rectangle {
    id: root

    property url imageSource: ""
    property string annotationJson: "{}"
    property var annotationData: ({ "regions": [] })
    property string toolMode: "select"
    property string selectedRegionId: ""

    property real zoomScale: 1.0
    property real panX: 0
    property real panY: 0

    property bool drawing: false
    property bool panning: false
    property bool movingRegion: false
    property bool resizingRegion: false
    property real startX: 0
    property real startY: 0
    property real currentX: 0
    property real currentY: 0
    property real panStartX: 0
    property real panStartY: 0
    property real panOriginX: 0
    property real panOriginY: 0
    property string movingRegionId: ""
    property string resizingRegionId: ""
    property int resizeHandleIndex: -1
    property var moveStartPoint: [0, 0]
    property var moveOriginalPoints: []
    property var moveCurrentPoints: []
    property var resizeOriginalPoints: []
    property var resizeCurrentPoints: []

    signal ocrBoxCreated(var points)
    signal layoutBoxCreated(var points)
    signal regionSelected(string regionId)
    signal regionMoved(string regionId, var points)
    signal regionDeleteRequested()

    color: "#0d0e10"
    focus: true

    Keys.onPressed: function(event) {
        if ((event.key === Qt.Key_Delete || event.key === Qt.Key_Backspace) && selectedRegionId !== "") {
            regionDeleteRequested()
            event.accepted = true
        }
    }

    onAnnotationJsonChanged: {
        try {
            annotationData = JSON.parse(annotationJson || "{}")
        } catch (err) {
            annotationData = { "regions": [] }
        }
        overlay.requestPaint()
    }

    onSelectedRegionIdChanged: overlay.requestPaint()
    onZoomScaleChanged: overlay.requestPaint()
    onPanXChanged: {
        overlay.requestPaint()
        viewportGrid.requestPaint()
    }
    onPanYChanged: {
        overlay.requestPaint()
        viewportGrid.requestPaint()
    }
    onImageSourceChanged: {
        resetView()
        overlay.requestPaint()
    }

    function resetView() {
        zoomScale = 1.0
        panX = 0
        panY = 0
    }

    function sourceWidth() {
        return Math.max(1, annotationData.width || pageImage.sourceSize.width || 1)
    }

    function sourceHeight() {
        return Math.max(1, annotationData.height || pageImage.sourceSize.height || 1)
    }

    function imagePaintRect() {
        return {
            "x": pageImage.x + (pageImage.width - pageImage.paintedWidth) / 2,
            "y": pageImage.y + (pageImage.height - pageImage.paintedHeight) / 2,
            "w": pageImage.paintedWidth,
            "h": pageImage.paintedHeight
        }
    }

    function mapPoint(point) {
        var rect = imagePaintRect()
        return {
            "x": rect.x + point[0] * rect.w / sourceWidth(),
            "y": rect.y + point[1] * rect.h / sourceHeight()
        }
    }

    function pointInsideImage(x, y) {
        var rect = imagePaintRect()
        return x >= rect.x && y >= rect.y && x <= rect.x + rect.w && y <= rect.y + rect.h
    }

    function localToImagePoint(x, y) {
        var rect = imagePaintRect()
        var px = Math.max(0, Math.min(sourceWidth(), (x - rect.x) * sourceWidth() / Math.max(1, rect.w)))
        var py = Math.max(0, Math.min(sourceHeight(), (y - rect.y) * sourceHeight() / Math.max(1, rect.h)))
        return [px, py]
    }

    function clonePoints(points) {
        var out = []
        for (var i = 0; i < points.length; ++i) {
            out.push([Number(points[i][0]), Number(points[i][1])])
        }
        return out
    }

    function clampPoint(point) {
        return [
            Math.max(0, Math.min(sourceWidth(), point[0])),
            Math.max(0, Math.min(sourceHeight(), point[1]))
        ]
    }

    function findRegionById(regionId) {
        var regions = annotationData.regions || []
        for (var i = 0; i < regions.length; ++i) {
            if ((regions[i].id || "") === regionId) {
                return regions[i]
            }
        }
        return null
    }

    function replaceRegionPoints(regionId, points) {
        var region = findRegionById(regionId)
        if (region) {
            if (region.type === "layout") {
                var minX = Math.min(points[0][0], points[1][0], points[2][0], points[3][0])
                var minY = Math.min(points[0][1], points[1][1], points[2][1], points[3][1])
                var maxX = Math.max(points[0][0], points[1][0], points[2][0], points[3][0])
                var maxY = Math.max(points[0][1], points[1][1], points[2][1], points[3][1])
                region.bbox = [minX, minY, maxX - minX, maxY - minY]
            } else {
                region.points = clonePoints(points)
            }
            overlay.requestPaint()
        }
    }

    function regionPoints(region) {
        if (region.type === "layout" && region.bbox && region.bbox.length >= 4) {
            var x = Number(region.bbox[0])
            var y = Number(region.bbox[1])
            var w = Number(region.bbox[2])
            var h = Number(region.bbox[3])
            return [[x, y], [x + w, y], [x + w, y + h], [x, y + h]]
        }
        return region.points || []
    }

    function regionAt(x, y) {
        var regions = annotationData.regions || []
        for (var i = regions.length - 1; i >= 0; --i) {
            var region = regions[i]
            var points = regionPoints(region)
            if ((region.type !== "ocr_text" && region.type !== "layout") || !points || points.length < 2) {
                continue
            }
            var minX = 999999
            var minY = 999999
            var maxX = -999999
            var maxY = -999999
            for (var p = 0; p < points.length; ++p) {
                var mapped = mapPoint(points[p])
                minX = Math.min(minX, mapped.x)
                minY = Math.min(minY, mapped.y)
                maxX = Math.max(maxX, mapped.x)
                maxY = Math.max(maxY, mapped.y)
            }
            if (x >= minX - 5 && y >= minY - 5 && x <= maxX + 5 && y <= maxY + 5) {
                return region.id || ""
            }
        }
        return ""
    }

    function selectedHandleAt(x, y) {
        var region = findRegionById(selectedRegionId)
        if (!region) {
            return -1
        }
        var points = regionPoints(region)
        for (var i = 0; i < points.length && i < 4; ++i) {
            var mapped = mapPoint(points[i])
            if (Math.abs(x - mapped.x) <= 8 && Math.abs(y - mapped.y) <= 8) {
                return i
            }
        }
        return -1
    }

    function resizedPoints(localX, localY) {
        var moving = localToImagePoint(localX, localY)
        var oppositeIndex = (resizeHandleIndex + 2) % 4
        var opposite = resizeOriginalPoints[oppositeIndex]
        var left = Math.min(moving[0], opposite[0])
        var right = Math.max(moving[0], opposite[0])
        var top = Math.min(moving[1], opposite[1])
        var bottom = Math.max(moving[1], opposite[1])
        left = Math.max(0, Math.min(sourceWidth(), left))
        right = Math.max(0, Math.min(sourceWidth(), right))
        top = Math.max(0, Math.min(sourceHeight(), top))
        bottom = Math.max(0, Math.min(sourceHeight(), bottom))
        if (Math.abs(right - left) < 2 || Math.abs(bottom - top) < 2) {
            return clonePoints(resizeOriginalPoints)
        }
        return [[left, top], [right, top], [right, bottom], [left, bottom]]
    }

    function draftLeft() { return Math.min(startX, currentX) }
    function draftTop() { return Math.min(startY, currentY) }
    function draftWidth() { return Math.abs(currentX - startX) }
    function draftHeight() { return Math.abs(currentY - startY) }

    function movedPoints(localX, localY) {
        var currentPoint = localToImagePoint(localX, localY)
        var dx = currentPoint[0] - moveStartPoint[0]
        var dy = currentPoint[1] - moveStartPoint[1]
        var out = []
        for (var i = 0; i < moveOriginalPoints.length; ++i) {
            out.push(clampPoint([moveOriginalPoints[i][0] + dx, moveOriginalPoints[i][1] + dy]))
        }
        return out
    }

    Canvas {
        id: viewportGrid
        anchors.fill: parent
        opacity: 0.55

        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            ctx.fillStyle = "#0d0e10"
            ctx.fillRect(0, 0, width, height)
            ctx.strokeStyle = "rgba(72, 78, 90, 0.22)"
            ctx.lineWidth = 1
            var gap = 48
            var offsetX = ((root.panX % gap) + gap) % gap
            var offsetY = ((root.panY % gap) + gap) % gap
            for (var x = offsetX; x < width; x += gap) {
                ctx.beginPath()
                ctx.moveTo(x, 0)
                ctx.lineTo(x, height)
                ctx.stroke()
            }
            for (var y = offsetY; y < height; y += gap) {
                ctx.beginPath()
                ctx.moveTo(0, y)
                ctx.lineTo(width, y)
                ctx.stroke()
            }
        }

        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
    }

    Item {
        id: content
        x: root.panX
        y: root.panY
        width: root.width
        height: root.height
        scale: root.zoomScale
        transformOrigin: Item.Center

        Image {
            id: pageImage
            anchors.fill: parent
            anchors.margins: 24
            fillMode: Image.PreserveAspectFit
            source: root.imageSource
            asynchronous: true
            smooth: true

            onStatusChanged: overlay.requestPaint()
            onPaintedWidthChanged: overlay.requestPaint()
            onPaintedHeightChanged: overlay.requestPaint()
        }

        Canvas {
            id: overlay
            anchors.fill: parent
            antialiasing: true

            onPaint: {
                var ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                if (pageImage.status === Image.Ready && pageImage.paintedWidth > 0 && pageImage.paintedHeight > 0) {
                    var imageRect = imagePaintRect()
                    ctx.strokeStyle = "rgba(214, 218, 226, 0.34)"
                    ctx.lineWidth = 1
                    ctx.strokeRect(imageRect.x - 0.5, imageRect.y - 0.5, imageRect.w + 1, imageRect.h + 1)
                }
                var regions = annotationData.regions || []
                ctx.font = "12px 'Microsoft YaHei UI', 'Segoe UI', sans-serif"
                for (var i = 0; i < regions.length; ++i) {
                    var region = regions[i]
                    var points = root.regionPoints(region)
                    if ((region.type !== "ocr_text" && region.type !== "layout") || !points || points.length < 2) {
                        continue
                    }
                    var first = mapPoint(points[0])
                    ctx.beginPath()
                    ctx.moveTo(first.x, first.y)
                    for (var p = 1; p < points.length; ++p) {
                        var mapped = mapPoint(points[p])
                        ctx.lineTo(mapped.x, mapped.y)
                    }
                    ctx.closePath()
                    var selected = (region.id || "") === selectedRegionId
                    ctx.lineWidth = selected ? 2.6 : 1.4
                    if (region.type === "layout") {
                        ctx.fillStyle = selected ? "rgba(47, 128, 237, 0.22)" : "rgba(20, 120, 109, 0.14)"
                        ctx.strokeStyle = selected ? "#60a5fa" : "#35a796"
                    } else {
                        ctx.fillStyle = selected ? "rgba(47, 128, 237, 0.22)" : (region.checked ? "rgba(70, 211, 154, 0.16)" : "rgba(245, 197, 107, 0.16)")
                        ctx.strokeStyle = selected ? "#60a5fa" : (region.checked ? "#46d39a" : "#f5c56b")
                    }
                    ctx.fill()
                    ctx.stroke()

                    var label = region.type === "layout" ? (region.label || "").trim() : (region.text || "").trim()
                    if (label.length > 24) {
                        label = label.slice(0, 24)
                    }
                    if (label.length > 0) {
                        var labelY = Math.max(14, first.y - 5)
                        var metrics = ctx.measureText(label)
                        ctx.fillStyle = "rgba(17, 18, 20, 0.92)"
                        ctx.fillRect(first.x, labelY - 15, metrics.width + 10, 18)
                        ctx.fillStyle = "#f4f6f8"
                        ctx.fillText(label, first.x + 5, labelY)
                    }

                    if (selected && points.length >= 4) {
                        ctx.fillStyle = "#f8fafc"
                        ctx.strokeStyle = "#111214"
                        ctx.lineWidth = 1
                        for (var h = 0; h < 4; ++h) {
                            var handle = mapPoint(points[h])
                            ctx.fillRect(handle.x - 5, handle.y - 5, 10, 10)
                            ctx.strokeRect(handle.x - 5, handle.y - 5, 10, 10)
                        }
                    }
                }
            }

            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
        }

        Rectangle {
            id: draft
            visible: root.drawing
            x: root.draftLeft()
            y: root.draftTop()
            width: root.draftWidth()
            height: root.draftHeight()
            color: Qt.rgba(0.96, 0.77, 0.42, 0.12)
            border.color: "#f5c56b"
            border.width: 2
        }
    }

    MouseArea {
        id: interaction
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
        hoverEnabled: true
        cursorShape: (root.toolMode === "drawOcr" || root.toolMode === "drawLayout") ? Qt.CrossCursor : (root.toolMode === "pan" ? Qt.OpenHandCursor : Qt.ArrowCursor)

        onWheel: function(wheel) {
            var factor = wheel.angleDelta.y > 0 ? 1.12 : 1 / 1.12
            root.zoomScale = Math.max(0.25, Math.min(6.0, root.zoomScale * factor))
            wheel.accepted = true
        }

        onDoubleClicked: function(mouse) {
            root.resetView()
            mouse.accepted = true
        }

        onPressed: function(mouse) {
            root.forceActiveFocus()
            var local = content.mapFromItem(interaction, mouse.x, mouse.y)
            if (root.toolMode === "pan" || mouse.button === Qt.RightButton || mouse.button === Qt.MiddleButton) {
                root.panning = true
                root.panStartX = mouse.x
                root.panStartY = mouse.y
                root.panOriginX = root.panX
                root.panOriginY = root.panY
                return
            }

            if (root.toolMode === "drawOcr" || root.toolMode === "drawLayout") {
                if (!root.pointInsideImage(local.x, local.y)) {
                    mouse.accepted = false
                    return
                }
                root.drawing = true
                root.startX = local.x
                root.startY = local.y
                root.currentX = local.x
                root.currentY = local.y
                return
            }

            var handleIndex = root.selectedHandleAt(local.x, local.y)
            if (handleIndex >= 0 && mouse.button === Qt.LeftButton) {
                var selectedRegion = root.findRegionById(root.selectedRegionId)
                root.resizingRegion = true
                root.resizingRegionId = root.selectedRegionId
                root.resizeHandleIndex = handleIndex
                root.resizeOriginalPoints = root.clonePoints(root.regionPoints(selectedRegion))
                root.resizeCurrentPoints = root.clonePoints(root.resizeOriginalPoints)
                return
            }

            var regionId = root.regionAt(local.x, local.y)
            root.selectedRegionId = regionId
            root.regionSelected(regionId)
            var region = root.findRegionById(regionId)
            var regionPoints = region ? root.regionPoints(region) : []
            if (region && regionPoints && regionPoints.length > 1 && mouse.button === Qt.LeftButton) {
                root.movingRegion = true
                root.movingRegionId = regionId
                root.moveStartPoint = root.localToImagePoint(local.x, local.y)
                root.moveOriginalPoints = root.clonePoints(regionPoints)
                root.moveCurrentPoints = root.clonePoints(regionPoints)
            }
        }

        onPositionChanged: function(mouse) {
            if (root.panning) {
                root.panX = root.panOriginX + mouse.x - root.panStartX
                root.panY = root.panOriginY + mouse.y - root.panStartY
                return
            }

            var local = content.mapFromItem(interaction, mouse.x, mouse.y)
            if (root.resizingRegion) {
                root.resizeCurrentPoints = root.resizedPoints(local.x, local.y)
                root.replaceRegionPoints(root.resizingRegionId, root.resizeCurrentPoints)
                return
            }

            if (root.movingRegion) {
                root.moveCurrentPoints = root.movedPoints(local.x, local.y)
                root.replaceRegionPoints(root.movingRegionId, root.moveCurrentPoints)
                return
            }

            if (!root.drawing) {
                return
            }
            var rect = root.imagePaintRect()
            root.currentX = Math.max(rect.x, Math.min(rect.x + rect.w, local.x))
            root.currentY = Math.max(rect.y, Math.min(rect.y + rect.h, local.y))
        }

        onReleased: function(mouse) {
            if (root.panning) {
                root.panning = false
                return
            }

            if (root.movingRegion) {
                root.movingRegion = false
                if (root.movingRegionId !== "" && root.moveCurrentPoints.length > 1) {
                    root.regionMoved(root.movingRegionId, root.moveCurrentPoints)
                }
                root.movingRegionId = ""
                root.moveOriginalPoints = []
                root.moveCurrentPoints = []
                return
            }

            if (root.resizingRegion) {
                root.resizingRegion = false
                if (root.resizingRegionId !== "" && root.resizeCurrentPoints.length > 1) {
                    root.regionMoved(root.resizingRegionId, root.resizeCurrentPoints)
                }
                root.resizingRegionId = ""
                root.resizeHandleIndex = -1
                root.resizeOriginalPoints = []
                root.resizeCurrentPoints = []
                return
            }

            if (!root.drawing) {
                return
            }
            root.drawing = false
            if (root.draftWidth() < 6 || root.draftHeight() < 6) {
                return
            }

            var left = root.draftLeft()
            var top = root.draftTop()
            var right = left + root.draftWidth()
            var bottom = top + root.draftHeight()
            var points = [
                root.localToImagePoint(left, top),
                root.localToImagePoint(right, top),
                root.localToImagePoint(right, bottom),
                root.localToImagePoint(left, bottom)
            ]
            if (root.toolMode === "drawLayout") {
                root.layoutBoxCreated(points)
            } else {
                root.ocrBoxCreated(points)
            }
        }
    }

    Rectangle {
        id: zoomBadge
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 14
        width: Math.max(64, zoomText.implicitWidth + 22)
        height: 30
        radius: 6
        color: "#18191c"
        border.color: "#383b43"
        visible: root.imageSource !== ""

        Text {
            id: zoomText
            anchors.centerIn: parent
            color: "#d6dae2"
            font.pixelSize: 12
            text: Math.round(root.zoomScale * 100) + "%"
        }
    }

    Text {
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.margins: 16
        color: "#9ca3af"
        font.pixelSize: 13
        text: root.imageSource === "" ? "未选择页面" : ""
    }
}
