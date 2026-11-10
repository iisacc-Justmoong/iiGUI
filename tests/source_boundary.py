"""Detect forbidden framework and higher-layer SDK dependencies in iiGUI."""
import re


def forbidden_component_rendering(source):
    """Flag common embedded-browser and OS-control implementations in UI surfaces.

    This is a lexical guard, not a proof about dynamically loaded code. Native
    windows, input methods, font services and accessibility are still allowed.
    """
    errors = []
    if re.search(r'\b(?:WKWebView|UIWebView|WebView2|ICoreWebView2(?:Controller)?|'
                 r'CefBrowser(?:Host)?|QWebEngineView)\b', source):
        errors.append("embedded browser used to render components")
    if re.search(r'\b(?:NSButton|NSTextField|NSStackView|NSControl|NSLayoutConstraint|'
                 r'NSImageView|NSScrollView|NSTableView|NSCollectionView|'
                 r'UIButton|UILabel|UITextField|UITextView|UIStackView|UIImageView|'
                 r'UISlider|UISwitch|UITableView|UICollectionView|UIScrollView)\b', source):
        errors.append("OS control or layout used to render components")
    control = r'(?:L?"(?:BUTTON|EDIT|STATIC|COMBOBOX|LISTBOX)"|WC_(?:BUTTON|EDIT|STATIC|COMBOBOX|LISTBOX)(?:W|A)?)'
    if re.search(r'\b(?:CreateWindowEx[AW]?\s*\(\s*[^,]*,\s*|CreateWindow[AW]?\s*\(\s*)'
                 + control + r'\s*[,)]', source, re.I):
        errors.append("Win32 control used to render components")
    return errors


def forbidden_source_dependencies(source):
    errors = []
    if re.search(r'^\s*#\s*(?:include|import)\s*[<"](?:LVUI|LVRS)/|'
                 r'\b(?:lvui|lvrs)::|namespace\s+(?:lvui|lvrs)\b', source, re.M | re.I):
        errors.append("lower-level host depends on LVUI or LVRS")
    if re.search(r'^\s*#\s*(?:include|import)\s*[<"](?:Qt|Q[A-Z]|SDL|WebKit|WebView|webkit|cef_|(?i:qml))',
                 source, re.M):
        errors.append("GUI-framework dependency")
    for match in re.finditer(r'^\s*#\s*(?:include|import)\s*[<"](ii[A-Z][A-Za-z0-9_]*)', source, re.M):
        if match.group(1) not in {"iiGUI", "iiSDKCore"}:
            errors.append("source depends on another iiSDK module: " + match.group(1))
    return errors


def forbidden_cmake_dependencies(source):
    errors = []
    if re.search(r'\b(?:find_package|find_dependency)\s*\(\s*"?'
                 r'(?:Qt\w*|QML|LVUI|LVRS|SDL\w*|WebKit|WebView|CEF)\b', source, re.I):
        errors.append("framework or higher-level SDK package dependency")
    for match in re.finditer(r'\b(?:find_package|find_dependency)\s*\(\s*"?(ii[A-Z][A-Za-z0-9_]*)',
                             source, re.I):
        if match.group(1).casefold() not in {"iigui", "iisdkcore"}:
            errors.append("package depends on another iiSDK module: " + match.group(1))
    for match in re.finditer(r'\btarget_link_libraries\s*\(\s*iiGUI\b', source):
        depth = 1
        end = match.end()
        while end < len(source) and depth:
            if source[end] == '(':
                depth += 1
            elif source[end] == ')':
                depth -= 1
            end += 1
        if re.search(r'(?<![A-Za-z0-9_])(?:-l|lib)?(?:LVUI|LVRS|Qt[56]?|QML)\w*(?:::|\b)',
                     source[match.end():end], re.I):
            errors.append("host target links a forbidden framework or SDK")
        for dependency in re.finditer(r'(?<![A-Za-z0-9_])(?:-l|lib)?(ii[A-Z][A-Za-z0-9_]*)(?:::|\b)',
                                      source[match.end():end]):
            if dependency.group(1) not in {"iiGUI", "iiSDKCore"}:
                errors.append("host target links another iiSDK module: " + dependency.group(1))
    return errors
