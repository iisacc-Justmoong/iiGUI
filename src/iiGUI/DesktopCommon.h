#pragma once
#include "Desktop.h"
#include "HostCommon.h"
#include <set>
namespace ii::gui::detail {
#ifdef __APPLE__
core::Result<ClipboardContent> read_pasteboard(void*);
#endif
inline bool valid_external_url(std::string_view value){
    return valid_title(value)&&value.size()<=8192&&(value.starts_with("https://")||value.starts_with("http://")||value.starts_with("mailto:")||value=="society://account");
}
inline bool valid_clipboard(const ClipboardContent& value){
    if(value.image.size()>64*1024*1024||value.files.size()>256||value.text.size()>1024*1024||value.url.size()>8192||!valid_title(value.text)||!valid_title(value.url))return false;
    if(!value.image.empty()&&value.image_mime!="image/png"&&value.image_mime!="image/tiff"&&value.image_mime!="image/jpeg"&&value.image_mime!="image/bmp")return false;
    return std::ranges::all_of(value.files,[](const auto& path){return !path.empty()&&path.size()<=32768&&valid_title(path);});
}
inline bool valid_dialog(const FileDialogOptions& value){
    if(unsigned(value.mode)>unsigned(FileDialogMode::directory)||!valid_title(value.title)||!valid_title(value.filename)||!valid_title(value.initial_directory)||value.filters.size()>32||(value.multiple&&value.mode!=FileDialogMode::open))return false;
    for(const auto& filter:value.filters){if(!valid_title(filter.title)||filter.extensions.empty()||filter.extensions.size()>64)return false;
        for(const auto& ext:filter.extensions)if(ext.empty()||ext.size()>32||!std::ranges::all_of(ext,[](unsigned char c){return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9');}))return false;}
    return true;
}
inline bool valid_menu(const std::vector<AppMenuItem>& menu){
    std::set<std::string> ids;std::size_t count=0;
    const auto walk=[&](auto&& self,const std::vector<AppMenuItem>& items,unsigned depth)->bool{
        if(depth>8)return false;
        for(const auto& item:items){if(++count>512||!valid_title(item.title)||!valid_title(item.id)||(!item.separator&&(item.id.empty()||!ids.insert(item.id).second)))return false;
            if(item.shortcut&&(item.shortcut->key.empty()||item.shortcut->key.size()>16||!valid_title(item.shortcut->key)))return false;
            if(!self(self,item.children,depth+1))return false;}
        return true;};return walk(walk,menu,0);
}
}
