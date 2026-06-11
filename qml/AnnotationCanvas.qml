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
    property bool editingHandle: false
    property bool rotatingRegion: false
    property bool temporaryPan: false
    property bool editInvalid: false
    property bool snapEnabled: true

    readonly property string tokenCanvasBackground: "#0B0D12"
    readonly property string tokenTextPrimary: "#E5E7EB"
    readonly property string tokenTextSecondary: "#9CA3AF"
    readonly property string tokenSurface: "#171A21"
    readonly property string tokenBorder: "#2A3140"
    readonly property string tokenSelected: "#38BDF8"
    readonly property string tokenOcr: "#22C55E"
    readonly property string tokenLayout: "#A855F7"
    readonly property string tokenUnchecked: "#F59E0B"
    readonly property string tokenError: "#EF4444"

    property real startX: 0
    property real startY: 0
    property real currentX: 0
    property real currentY: 0
    property real panStartX: 0
    property real panStartY: 0
    property real panOriginX: 0
    property real panOriginY: 0
    property real rotateStartAngle: 0
    property real rotateCurrentAngle: 0
    property string previousToolMode: "select"
    property string movingRegionId: ""
    property string editingRegionId: ""
    property int editHandleIndex: -1
    property var moveStartPoint: [0, 0]
    property var moveOriginalPoints: []
    property var moveCurrentPoints: []
    property var editOriginalPoints: []
    property var editCurrentPoints: []
    property var rotateCenter: [0, 0]
    property var activeGuides: []

    signal ocrBoxCreated(var points)
    signal layoutBoxCreated(var points)
    signal regionSelected(string regionId)
    signal regionMoved(string regionId, var points)
    signal regionDeleteRequested()
    signal toolModeRequested(string mode)
    signal undoRequested()
    signal redoRequested()
    signal regionConfirmRequested()
    signal regionCheckedToggleRequested()
    signal regionIgnoreToggleRequested()
    signal canvasMessage(string message)

    color: tokenCanvasBackground
    focus: true

    Keys.onPressed: function(event) {
        if (event.isAutoRepeat) {
            return
        }
        if (hasControl(event) && event.key === Qt.Key_Z && !(event.modifiers & Qt.ShiftModifier)) {
            undoRequested()
            event.accepted = true
            return
        }
        if ((hasControl(event) && event.key === Qt.Key_Y)
                || (hasControl(event) && event.key === Qt.Key_Z && (event.modifiers & Qt.ShiftModifier))) {
            redoRequested()
            event.accepted = true
            return
        }
        if ((event.key === Qt.Key_Delete || event.key === Qt.Key_Backspace) && selectedRegionId !== "") {
            regionDeleteRequested()
            event.accepted = true
            return
        }
        if (event.key === Qt.Key_Space) {
            if (!temporaryPan) {
                previousToolMode = toolMode
                temporaryPan = true
                toolModeRequested("pan")
            }
            event.accepted = true
            return
        }
        if (hasControl(event)) {
            return
        }
        if (event.key === Qt.Key_V) {
            toolModeRequested("select")
            event.accepted = true
        } else if (event.key === Qt.Key_B) {
            toolModeRequested("drawOcr")
            event.accepted = true
        } else if (event.key === Qt.Key_L) {
            toolModeRequested("drawLayout")
            event.accepted = true
        } else if (event.key === Qt.Key_H) {
            toolModeRequested("pan")
            event.accepted = true
        } else if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter) && selectedRegionId !== "") {
            regionConfirmRequested()
            event.accepted = true
        } else if (event.key === Qt.Key_C && selectedRegionId !== "") {
            regionCheckedToggleRequested()
            event.accepted = true
        } else if (event.key === Qt.Key_I && selectedRegionId !== "") {
            regionIgnoreToggleRequested()
            event.accepted = true
        } else if (event.key === Qt.Key_Escape) {
            cancelLiveEdit()
            toolModeRequested("select")
            event.accepted = true
        }
    }

    Keys.onReleased: function(event) {
        if (event.key === Qt.Key_Space && temporaryPan) {
            temporaryPan = false
            toolModeRequested(previousToolMode)
            event.accepted = true
        }
    }

    onAnnotationJsonChanged: {
        try {
            annotationData = JSON.parse(annotationJson || "{}")
        } catch (err) {
            annotationData = { "regions": [] }
        }
        activeGuides = []
        editInvalid = false
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

    function hasControl(event) {
        return (event.modifiers & Qt.ControlModifier) || (event.modifiers & Qt.MetaModifier)
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
            Math.max(0, Math.min(sourceWidth(), Number(point[0]))),
            Math.max(0, Math.min(sourceHeight(), Number(point[1])))
        ]
    }

    function polygonArea(points) {
        var area = 0
        for (var i = 0; i < points.length; ++i) {
            var a = points[i]
            var b = points[(i + 1) % points.length]
            area += a[0] * b[1] - b[0] * a[1]
        }
        return area / 2
    }

    function cross(a, b, c) {
        return (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])
    }

    function rangesOverlap(a1, a2, b1, b2) {
        return Math.max(Math.min(a1, a2), Math.min(b1, b2)) <= Math.min(Math.max(a1, a2), Math.max(b1, b2)) + 0.001
    }

    function segmentsIntersect(a, b, c, d) {
        var c1 = cross(a, b, c)
        var c2 = cross(a, b, d)
        var c3 = cross(c, d, a)
        var c4 = cross(c, d, b)
        if (Math.abs(c1) < 0.001 && Math.abs(c2) < 0.001 && Math.abs(c3) < 0.001 && Math.abs(c4) < 0.001) {
            return rangesOverlap(a[0], b[0], c[0], d[0]) && rangesOverlap(a[1], b[1], c[1], d[1])
        }
        return c1 * c2 <= 0 && c3 * c4 <= 0
    }

    function validPolygon(points) {
        if (!points || points.length < 4) {
            return false
        }
        for (var i = 0; i < points.length; ++i) {
            var a = points[i]
            var b = points[(i + 1) % points.length]
            if (Math.hypot(b[0] - a[0], b[1] - a[1]) < 1) {
                return false
            }
        }
        if (Math.abs(polygonArea(points)) < 4) {
            return false
        }
        for (var e1 = 0; e1 < points.length; ++e1) {
            var e1b = (e1 + 1) % points.length
            for (var e2 = e1 + 1; e2 < points.length; ++e2) {
                var e2b = (e2 + 1) % points.length
                if (e1 === e2 || e1b === e2 || e2b === e1 || (e1 === 0 && e2b === 0)) {
                    continue
                }
                if (segmentsIntersect(points[e1], points[e1b], points[e2], points[e2b])) {
                    return false
                }
            }
        }
        return true
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

    function bboxFromPoints(points) {
        var minX = 999999999
        var minY = 999999999
        var maxX = -999999999
        var maxY = -999999999
        for (var i = 0; i < points.length; ++i) {
            minX = Math.min(minX, points[i][0])
            minY = Math.min(minY, points[i][1])
            maxX = Math.max(maxX, points[i][0])
            maxY = Math.max(maxY, points[i][1])
        }
        return [minX, minY, Math.max(1, maxX - minX), Math.max(1, maxY - minY)]
    }

    function replaceRegionPoints(regionId, points) {
        var region = findRegionById(regionId)
        if (!region) {
            return
        }
        var normalized = []
        for (var i = 0; i < points.length; ++i) {
            normalized.push(clampPoint(points[i]))
        }
        region.points = clonePoints(normalized)
        region.shape = normalized.length === 4 ? "quad" : "polygon"
        if (region.type === "layout") {
            region.bbox = bboxFromPoints(normalized)
        }
        editInvalid = !validPolygon(normalized)
        overlay.requestPaint()
    }

    function regionPoints(region) {
        if (region.points && region.points.length >= 2) {
            return clonePoints(region.points)
        }
        if (region.type === "layout" && region.bbox && region.bbox.length >= 4) {
            var x = Number(region.bbox[0])
            var y = Number(region.bbox[1])
            var w = Number(region.bbox[2])
            var h = Number(region.bbox[3])
            return [[x, y], [x + w, y], [x + w, y + h], [x, y + h]]
        }
        return []
    }

    function pointInPolygon(x, y, mappedPoints) {
        var inside = false
        for (var i = 0, j = mappedPoints.length - 1; i < mappedPoints.length; j = i++) {
            var pi = mappedPoints[i]
            var pj = mappedPoints[j]
            var intersects = ((pi.y > y) !== (pj.y > y))
                    && (x < (pj.x - pi.x) * (y - pi.y) / Math.max(0.0001, pj.y - pi.y) + pi.x)
            if (intersects) {
                inside = !inside
            }
        }
        return inside
    }

    function distanceToSegment(px, py, ax, ay, bx, by) {
        var dx = bx - ax
        var dy = by - ay
        if (Math.abs(dx) < 0.001 && Math.abs(dy) < 0.001) {
            return Math.hypot(px - ax, py - ay)
        }
        var t = ((px - ax) * dx + (py - ay) * dy) / (dx * dx + dy * dy)
        t = Math.max(0, Math.min(1, t))
        return Math.hypot(px - (ax + t * dx), py - (ay + t * dy))
    }

    function regionAt(x, y) {
        var regions = annotationData.regions || []
        for (var i = regions.length - 1; i >= 0; --i) {
            var region = regions[i]
            var points = regionPoints(region)
            if ((region.type !== "ocr_text" && region.type !== "layout") || !points || points.length < 3) {
                continue
            }
            var mapped = []
            for (var p = 0; p < points.length; ++p) {
                mapped.push(mapPoint(points[p]))
            }
            if (pointInPolygon(x, y, mapped)) {
                return region.id || ""
            }
            for (var e = 0; e < mapped.length; ++e) {
                var a = mapped[e]
                var b = mapped[(e + 1) % mapped.length]
                if (distanceToSegment(x, y, a.x, a.y, b.x, b.y) <= 5) {
                    return region.id || ""
                }
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
        for (var i = 0; i < points.length; ++i) {
            var mapped = mapPoint(points[i])
            if (Math.abs(x - mapped.x) <= 8 && Math.abs(y - mapped.y) <= 8) {
                return i
            }
        }
        return -1
    }

    function selectedEdgeAt(x, y) {
        var region = findRegionById(selectedRegionId)
        if (!region) {
            return -1
        }
        var points = regionPoints(region)
        if (points.length < 4) {
            return -1
        }
        var bestIndex = -1
        var bestDistance = 999999
        for (var i = 0; i < points.length; ++i) {
            var a = mapPoint(points[i])
            var b = mapPoint(points[(i + 1) % points.length])
            var distance = distanceToSegment(x, y, a.x, a.y, b.x, b.y)
            if (distance < bestDistance) {
                bestDistance = distance
                bestIndex = i
            }
        }
        return bestDistance <= 8 ? bestIndex : -1
    }

    function polygonCenter(points) {
        var cx = 0
        var cy = 0
        for (var i = 0; i < points.length; ++i) {
            cx += points[i][0]
            cy += points[i][1]
        }
        return [cx / Math.max(1, points.length), cy / Math.max(1, points.length)]
    }

    function selectedBounds(points) {
        var minX = 999999
        var minY = 999999
        var maxX = -999999
        var maxY = -999999
        for (var i = 0; i < points.length; ++i) {
            var mapped = mapPoint(points[i])
            minX = Math.min(minX, mapped.x)
            minY = Math.min(minY, mapped.y)
            maxX = Math.max(maxX, mapped.x)
            maxY = Math.max(maxY, mapped.y)
        }
        return {"x": minX, "y": minY, "w": maxX - minX, "h": maxY - minY}
    }

    function rotationHandlePosition(points) {
        var bounds = selectedBounds(points)
        return {"x": bounds.x + bounds.w / 2, "y": bounds.y - 30}
    }

    function rotationHandleAt(x, y) {
        var region = findRegionById(selectedRegionId)
        if (!region) {
            return false
        }
        var points = regionPoints(region)
        if (points.length < 4) {
            return false
        }
        var handle = rotationHandlePosition(points)
        return Math.hypot(x - handle.x, y - handle.y) <= 10
    }

    function snapThresholds() {
        var rect = imagePaintRect()
        var localPx = 9 / Math.max(0.25, zoomScale)
        return {
            "x": localPx * sourceWidth() / Math.max(1, rect.w),
            "y": localPx * sourceHeight() / Math.max(1, rect.h)
        }
    }

    function snapAxisCandidates(regionId) {
        var xs = [0, sourceWidth()]
        var ys = [0, sourceHeight()]
        var regions = annotationData.regions || []
        for (var i = 0; i < regions.length; ++i) {
            var region = regions[i]
            if ((region.id || "") === regionId) {
                continue
            }
            var points = regionPoints(region)
            for (var p = 0; p < points.length; ++p) {
                xs.push(Number(points[p][0]))
                ys.push(Number(points[p][1]))
            }
        }
        return {"xs": xs, "ys": ys}
    }

    function snapSinglePoint(point, regionId) {
        var clamped = clampPoint(point)
        if (!snapEnabled) {
            activeGuides = []
            return clamped
        }
        var thresholds = snapThresholds()
        var candidates = snapAxisCandidates(regionId)
        var bestX = clamped[0]
        var bestY = clamped[1]
        var bestDx = thresholds.x
        var bestDy = thresholds.y
        var guides = []
        for (var i = 0; i < candidates.xs.length; ++i) {
            var dx = Math.abs(clamped[0] - candidates.xs[i])
            if (dx <= bestDx) {
                bestDx = dx
                bestX = candidates.xs[i]
            }
        }
        for (var j = 0; j < candidates.ys.length; ++j) {
            var dy = Math.abs(clamped[1] - candidates.ys[j])
            if (dy <= bestDy) {
                bestDy = dy
                bestY = candidates.ys[j]
            }
        }
        if (bestX !== clamped[0]) {
            guides.push({"axis": "x", "value": bestX})
        }
        if (bestY !== clamped[1]) {
            guides.push({"axis": "y", "value": bestY})
        }
        activeGuides = guides
        return clampPoint([bestX, bestY])
    }

    function clampTranslation(points) {
        var minX = 999999
        var minY = 999999
        var maxX = -999999
        var maxY = -999999
        for (var i = 0; i < points.length; ++i) {
            minX = Math.min(minX, points[i][0])
            minY = Math.min(minY, points[i][1])
            maxX = Math.max(maxX, points[i][0])
            maxY = Math.max(maxY, points[i][1])
        }
        var dx = 0
        var dy = 0
        if (minX < 0) dx = -minX
        if (maxX + dx > sourceWidth()) dx = sourceWidth() - maxX
        if (minY < 0) dy = -minY
        if (maxY + dy > sourceHeight()) dy = sourceHeight() - maxY
        var out = []
        for (var p = 0; p < points.length; ++p) {
            out.push([points[p][0] + dx, points[p][1] + dy])
        }
        return out
    }

    function snapTranslatedPoints(points, regionId) {
        var clamped = clampTranslation(points)
        if (!snapEnabled) {
            activeGuides = []
            return clamped
        }
        var thresholds = snapThresholds()
        var candidates = snapAxisCandidates(regionId)
        var bestDx = 0
        var bestDy = 0
        var bestAbsX = thresholds.x
        var bestAbsY = thresholds.y
        var guides = []
        for (var p = 0; p < clamped.length; ++p) {
            for (var x = 0; x < candidates.xs.length; ++x) {
                var dx = candidates.xs[x] - clamped[p][0]
                if (Math.abs(dx) <= bestAbsX) {
                    bestAbsX = Math.abs(dx)
                    bestDx = dx
                    guides[0] = {"axis": "x", "value": candidates.xs[x]}
                }
            }
            for (var y = 0; y < candidates.ys.length; ++y) {
                var dy = candidates.ys[y] - clamped[p][1]
                if (Math.abs(dy) <= bestAbsY) {
                    bestAbsY = Math.abs(dy)
                    bestDy = dy
                    guides[1] = {"axis": "y", "value": candidates.ys[y]}
                }
            }
        }
        var out = []
        for (var i = 0; i < clamped.length; ++i) {
            out.push([clamped[i][0] + bestDx, clamped[i][1] + bestDy])
        }
        activeGuides = guides.filter(function(item) { return item !== undefined })
        return clampTranslation(out)
    }

    function editedPoints(localX, localY) {
        var out = clonePoints(editOriginalPoints)
        if (editHandleIndex < 0 || editHandleIndex >= out.length) {
            return out
        }
        out[editHandleIndex] = snapSinglePoint(localToImagePoint(localX, localY), editingRegionId)
        return out
    }

    function movedPoints(localX, localY) {
        var currentPoint = localToImagePoint(localX, localY)
        var dx = currentPoint[0] - moveStartPoint[0]
        var dy = currentPoint[1] - moveStartPoint[1]
        var raw = []
        for (var i = 0; i < moveOriginalPoints.length; ++i) {
            raw.push([moveOriginalPoints[i][0] + dx, moveOriginalPoints[i][1] + dy])
        }
        return snapTranslatedPoints(raw, movingRegionId)
    }

    function rotatedPoints(localX, localY) {
        var point = localToImagePoint(localX, localY)
        var angle = Math.atan2(point[1] - rotateCenter[1], point[0] - rotateCenter[0])
        var delta = angle - rotateStartAngle
        rotateCurrentAngle = delta
        var cosv = Math.cos(delta)
        var sinv = Math.sin(delta)
        var out = []
        for (var i = 0; i < editOriginalPoints.length; ++i) {
            var x = editOriginalPoints[i][0] - rotateCenter[0]
            var y = editOriginalPoints[i][1] - rotateCenter[1]
            out.push([
                rotateCenter[0] + x * cosv - y * sinv,
                rotateCenter[1] + x * sinv + y * cosv
            ])
        }
        activeGuides = []
        return out
    }

    function addPolygonPoint(edgeIndex, imagePoint) {
        var region = findRegionById(selectedRegionId)
        if (!region) {
            return
        }
        var points = regionPoints(region)
        if (edgeIndex < 0 || points.length < 4) {
            return
        }
        points.splice(edgeIndex + 1, 0, snapSinglePoint(imagePoint, selectedRegionId))
        if (!validPolygon(points)) {
            canvasMessage("Point was not added: polygon would be invalid")
            return
        }
        replaceRegionPoints(selectedRegionId, points)
        regionMoved(selectedRegionId, points)
        canvasMessage("Polygon point added")
    }

    function removePolygonPoint(handleIndex) {
        var region = findRegionById(selectedRegionId)
        if (!region) {
            return
        }
        var points = regionPoints(region)
        if (points.length <= 4 || handleIndex < 0 || handleIndex >= points.length) {
            canvasMessage("Polygon needs at least 4 points")
            return
        }
        points.splice(handleIndex, 1)
        if (!validPolygon(points)) {
            canvasMessage("Point was not removed: polygon would be invalid")
            return
        }
        replaceRegionPoints(selectedRegionId, points)
        regionMoved(selectedRegionId, points)
        canvasMessage("Polygon point removed")
    }

    function cancelLiveEdit() {
        drawing = false
        panning = false
        movingRegion = false
        editingHandle = false
        rotatingRegion = false
        activeGuides = []
        editInvalid = false
        if (editingRegionId !== "" && editOriginalPoints.length > 0) {
            replaceRegionPoints(editingRegionId, editOriginalPoints)
        }
        if (movingRegionId !== "" && moveOriginalPoints.length > 0) {
            replaceRegionPoints(movingRegionId, moveOriginalPoints)
        }
        movingRegionId = ""
        editingRegionId = ""
        editHandleIndex = -1
    }

    function draftLeft() { return Math.min(startX, currentX) }
    function draftTop() { return Math.min(startY, currentY) }
    function draftWidth() { return Math.abs(currentX - startX) }
    function draftHeight() { return Math.abs(currentY - startY) }

    Canvas {
        id: viewportGrid
        anchors.fill: parent
        opacity: 0.55

        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            ctx.fillStyle = root.tokenCanvasBackground
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
                var imageRect = root.imagePaintRect()
                if (pageImage.status === Image.Ready && pageImage.paintedWidth > 0 && pageImage.paintedHeight > 0) {
                    ctx.strokeStyle = "rgba(214, 218, 226, 0.34)"
                    ctx.lineWidth = 1
                    ctx.strokeRect(imageRect.x - 0.5, imageRect.y - 0.5, imageRect.w + 1, imageRect.h + 1)
                }

                var regions = root.annotationData.regions || []
                ctx.font = "12px 'Microsoft YaHei UI', 'Segoe UI', sans-serif"
                for (var i = 0; i < regions.length; ++i) {
                    var region = regions[i]
                    var points = root.regionPoints(region)
                    if ((region.type !== "ocr_text" && region.type !== "layout") || !points || points.length < 3) {
                        continue
                    }
                    var first = root.mapPoint(points[0])
                    ctx.beginPath()
                    ctx.moveTo(first.x, first.y)
                    for (var p = 1; p < points.length; ++p) {
                        var mapped = root.mapPoint(points[p])
                        ctx.lineTo(mapped.x, mapped.y)
                    }
                    ctx.closePath()
                    var selected = (region.id || "") === root.selectedRegionId
                    var invalidSelected = selected && root.editInvalid
                    ctx.lineWidth = selected ? 2.6 : 1.4
                    if (invalidSelected) {
                        ctx.fillStyle = "rgba(239, 68, 68, 0.18)"
                        ctx.strokeStyle = root.tokenError
                    } else if (region.type === "layout") {
                        ctx.fillStyle = selected ? "rgba(47, 128, 237, 0.22)" : "rgba(168, 85, 247, 0.12)"
                        ctx.strokeStyle = selected ? root.tokenSelected : root.tokenLayout
                    } else {
                        ctx.fillStyle = selected ? "rgba(47, 128, 237, 0.22)" : (region.checked ? "rgba(70, 211, 154, 0.16)" : "rgba(245, 197, 107, 0.16)")
                        ctx.strokeStyle = selected ? root.tokenSelected : (region.checked ? root.tokenOcr : root.tokenUnchecked)
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
                        ctx.fillStyle = root.tokenTextPrimary
                        ctx.fillText(label, first.x + 5, labelY)
                    }

                    if (selected && points.length >= 4) {
                        ctx.fillStyle = root.tokenTextPrimary
                        ctx.strokeStyle = root.tokenCanvasBackground
                        ctx.lineWidth = 1
                        for (var h = 0; h < points.length; ++h) {
                            var handle = root.mapPoint(points[h])
                            ctx.fillRect(handle.x - 5, handle.y - 5, 10, 10)
                            ctx.strokeRect(handle.x - 5, handle.y - 5, 10, 10)
                        }
                        var rotate = root.rotationHandlePosition(points)
                        var top = root.selectedBounds(points)
                        ctx.strokeStyle = root.tokenSelected
                        ctx.beginPath()
                        ctx.moveTo(top.x + top.w / 2, top.y)
                        ctx.lineTo(rotate.x, rotate.y)
                        ctx.stroke()
                        ctx.beginPath()
                        ctx.arc(rotate.x, rotate.y, 8, 0, Math.PI * 2)
                        ctx.fillStyle = root.tokenCanvasBackground
                        ctx.fill()
                        ctx.strokeStyle = root.tokenTextPrimary
                        ctx.stroke()
                    }
                }

                if (root.activeGuides.length > 0 && pageImage.status === Image.Ready) {
                    if (ctx.setLineDash) {
                        ctx.setLineDash([6, 5])
                    }
                    ctx.strokeStyle = "rgba(56, 189, 248, 0.85)"
                    ctx.lineWidth = 1
                    for (var g = 0; g < root.activeGuides.length; ++g) {
                        var guide = root.activeGuides[g]
                        ctx.beginPath()
                        if (guide.axis === "x") {
                            var mappedX = root.mapPoint([guide.value, 0]).x
                            ctx.moveTo(mappedX, imageRect.y)
                            ctx.lineTo(mappedX, imageRect.y + imageRect.h)
                        } else {
                            var mappedY = root.mapPoint([0, guide.value]).y
                            ctx.moveTo(imageRect.x, mappedY)
                            ctx.lineTo(imageRect.x + imageRect.w, mappedY)
                        }
                        ctx.stroke()
                    }
                    if (ctx.setLineDash) {
                        ctx.setLineDash([])
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
            border.color: root.tokenUnchecked
            border.width: 2
        }
    }

    MouseArea {
        id: interaction
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
        hoverEnabled: true
        cursorShape: (root.toolMode === "drawOcr" || root.toolMode === "drawLayout")
            ? Qt.CrossCursor
            : (root.toolMode === "pan" ? Qt.OpenHandCursor : Qt.ArrowCursor)

        onWheel: function(wheel) {
            var factor = wheel.angleDelta.y > 0 ? 1.12 : 1 / 1.12
            root.zoomScale = Math.max(0.25, Math.min(6.0, root.zoomScale * factor))
            wheel.accepted = true
        }

        onDoubleClicked: function(mouse) {
            root.forceActiveFocus()
            var local = content.mapFromItem(interaction, mouse.x, mouse.y)
            var edgeIndex = root.selectedEdgeAt(local.x, local.y)
            if ((mouse.modifiers & Qt.ControlModifier) && edgeIndex >= 0) {
                root.addPolygonPoint(edgeIndex, root.localToImagePoint(local.x, local.y))
            } else {
                root.resetView()
            }
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
                root.activeGuides = []
                root.startX = local.x
                root.startY = local.y
                root.currentX = local.x
                root.currentY = local.y
                return
            }

            var handleIndex = root.selectedHandleAt(local.x, local.y)
            if ((mouse.modifiers & Qt.AltModifier) && handleIndex >= 0) {
                root.removePolygonPoint(handleIndex)
                return
            }

            var edgeIndex = root.selectedEdgeAt(local.x, local.y)
            if ((mouse.modifiers & Qt.ControlModifier) && edgeIndex >= 0) {
                root.addPolygonPoint(edgeIndex, root.localToImagePoint(local.x, local.y))
                return
            }

            if (root.rotationHandleAt(local.x, local.y) && mouse.button === Qt.LeftButton) {
                var rotateRegion = root.findRegionById(root.selectedRegionId)
                root.rotatingRegion = true
                root.editingRegionId = root.selectedRegionId
                root.editOriginalPoints = root.clonePoints(root.regionPoints(rotateRegion))
                root.editCurrentPoints = root.clonePoints(root.editOriginalPoints)
                root.rotateCenter = root.polygonCenter(root.editOriginalPoints)
                var rotatePoint = root.localToImagePoint(local.x, local.y)
                root.rotateStartAngle = Math.atan2(rotatePoint[1] - root.rotateCenter[1], rotatePoint[0] - root.rotateCenter[0])
                root.rotateCurrentAngle = 0
                return
            }

            if (handleIndex >= 0 && mouse.button === Qt.LeftButton) {
                var selectedRegion = root.findRegionById(root.selectedRegionId)
                root.editingHandle = true
                root.editingRegionId = root.selectedRegionId
                root.editHandleIndex = handleIndex
                root.editOriginalPoints = root.clonePoints(root.regionPoints(selectedRegion))
                root.editCurrentPoints = root.clonePoints(root.editOriginalPoints)
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
            if (root.editingHandle) {
                root.editCurrentPoints = root.editedPoints(local.x, local.y)
                root.replaceRegionPoints(root.editingRegionId, root.editCurrentPoints)
                return
            }

            if (root.rotatingRegion) {
                root.editCurrentPoints = root.rotatedPoints(local.x, local.y)
                root.replaceRegionPoints(root.editingRegionId, root.editCurrentPoints)
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
                root.activeGuides = []
                if (root.movingRegionId !== "" && root.moveCurrentPoints.length >= 4 && root.validPolygon(root.moveCurrentPoints)) {
                    root.regionMoved(root.movingRegionId, root.moveCurrentPoints)
                } else if (root.movingRegionId !== "") {
                    root.replaceRegionPoints(root.movingRegionId, root.moveOriginalPoints)
                    root.canvasMessage("Region move was rejected: invalid polygon")
                }
                root.movingRegionId = ""
                root.moveOriginalPoints = []
                root.moveCurrentPoints = []
                root.editInvalid = false
                return
            }

            if (root.editingHandle || root.rotatingRegion) {
                var regionId = root.editingRegionId
                var points = root.editCurrentPoints
                root.editingHandle = false
                root.rotatingRegion = false
                root.activeGuides = []
                if (regionId !== "" && points.length >= 4 && root.validPolygon(points)) {
                    root.regionMoved(regionId, points)
                } else if (regionId !== "") {
                    root.replaceRegionPoints(regionId, root.editOriginalPoints)
                    root.canvasMessage("Region edit was rejected: invalid polygon")
                }
                root.editingRegionId = ""
                root.editHandleIndex = -1
                root.editOriginalPoints = []
                root.editCurrentPoints = []
                root.editInvalid = false
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
            if (!root.validPolygon(points)) {
                root.canvasMessage("Box was not created: invalid polygon")
                return
            }
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
        color: root.tokenSurface
        border.color: root.tokenBorder
        visible: root.imageSource !== ""

        Text {
            id: zoomText
            anchors.centerIn: parent
            color: root.tokenTextPrimary
            font.pixelSize: 12
            text: Math.round(root.zoomScale * 100) + "%"
        }
    }

    Text {
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.margins: 16
        color: root.tokenTextSecondary
        font.pixelSize: 13
        text: root.imageSource === "" ? "No page selected" : ""
    }
}
