.pragma library

// freedesktop icon name -> Kos.Icons glyph name.
//
// Spectacle names its tools by the standard icon theme spec. We keep those names
// at the call sites so the QML still reads like upstream, and translate them
// here into the pinned NextKde font. Anything missing from the font is mapped to
// the closest available glyph instead of silently rendering nothing.
var names = {
    "arrow-down": "arrow_down",
    "arrow-up": "arrow_up",
    "blur": "drop",
    "camera-photo": "camera",
    "camera-video": "videocam",
    "color-management": "circle_lefthalf_fill",
    "color-picker": "eyedropper",
    "configure": "gear",
    "dialog-cancel": "xmark_circle",
    "dialog-close": "xmark",
    "dialog-ok": "checkmark",
    "document-edit": "pencil",
    "document-open": "folder",
    "document-open-folder": "folder",
    "document-open-recent": "clock",
    "document-save": "arrow_down_doc",
    "document-save-as": "arrow_down_doc",
    "document-scan": "viewfinder",
    "document-share": "arrow_up_doc",
    "draw-arrow": "arrow_up_right",
    "draw-ellipse": "circle",
    "draw-freehand": "scribble",
    "draw-highlight": "pencil_outline",
    "draw-line": "minus",
    "draw-number": "number_circle",
    "draw-rectangle": "rectangle",
    "draw-text": "textformat",
    "edit-copy": "square_on_square",
    "edit-copy-path": "doc_on_doc",
    "edit-image": "photo",
    "edit-paste": "doc_on_clipboard",
    "edit-redo": "arrow_uturn_right",
    "edit-select": "cursor_rays",
    "edit-undo": "arrow_uturn_left",
    "help-contents": "question_circle",
    "list-add": "plus",
    "media-playback-pause": "pause",
    "media-playback-start": "play",
    "media-record": "recordingtape",
    "pixelate": "square_grid_2x2",
    "security-high": "checkmark_shield",
    "transform-browse": "viewfinder",
    "transform-crop": "crop",
    "transform-scale": "arrow_up_left_arrow_down_right",
    "view-barcode-qr": "qrcode",
    "view-fullscreen": "arrow_up_left_arrow_down_right",
    "view-more-symbolic": "ellipsis",
    "view-preview": "eye",
    "window-close": "xmark",
    "window-pin": "pin",
    "zoom-in": "plus_circle",
    "zoom-out": "minus_circle"
}

function glyph(name) {
    return names[name] || name
}
