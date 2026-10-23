#pragma once
#include <cctype>
#include <map>
#include <string>
#include <vector>

namespace papernx::update_json {
// Bounded JSON reader for the GitHub release response. Object lookups only
// inspect direct children, so notes/author fields cannot masquerade as assets.
struct Value {
    enum Kind {Null,String,Number,Bool,Object,Array} kind{Null};
    std::string text;
    std::map<std::string,Value> fields;
    std::vector<Value> items;
    const Value &get(const char *name) const {auto it=fields.find(name);static const Value empty;return it==fields.end()?empty:it->second;}
    std::string str() const {return kind==String?text:std::string{};}
    bool yes() const {return kind==Bool && text=="true";}
};
class Parser {
    const std::string &input_;std::size_t pos_{0},nodes_{0};
    void space(){while(pos_<input_.size() && (input_[pos_]==' ' || input_[pos_]=='\r' || input_[pos_]=='\n' || input_[pos_]=='\t'))++pos_;}
    bool take(char c){space();if(pos_==input_.size() || input_[pos_]!=c)return false;++pos_;return true;}
    int hex4(){int value=0;for(int i=0;i<4;++i){if(pos_==input_.size())return -1;char c=input_[pos_++];int n=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;if(n<0)return -1;value=value*16+n;}return value;}
    static void utf8(std::string &out,unsigned c){if(c<0x80)out+=char(c);else if(c<0x800){out+=char(0xc0|(c>>6));out+=char(0x80|(c&63));}else if(c<0x10000){out+=char(0xe0|(c>>12));out+=char(0x80|((c>>6)&63));out+=char(0x80|(c&63));}else{out+=char(0xf0|(c>>18));out+=char(0x80|((c>>12)&63));out+=char(0x80|((c>>6)&63));out+=char(0x80|(c&63));}}
    bool string(std::string &out){
        if(!take('"'))return false;
        while(pos_<input_.size()){
            unsigned char c=input_[pos_++];if(c=='"')return true;if(c<32)return false;
            if(c!='\\'){out+=char(c);continue;}if(pos_==input_.size())return false;c=input_[pos_++];
            if(c=='"'||c=='\\'||c=='/')out+=char(c);
            else if(c=='n')out+='\n';else if(c=='r')out+='\r';else if(c=='t')out+='\t';else if(c=='b')out+='\b';else if(c=='f')out+='\f';
            else if(c=='u'){int n=hex4();if(n<0)return false;if(n>=0xd800&&n<=0xdbff){if(pos_+2>input_.size()||input_[pos_]!='\\'||input_[pos_+1]!='u')return false;pos_+=2;int low=hex4();if(low<0xdc00||low>0xdfff)return false;n=0x10000+((n-0xd800)<<10)+low-0xdc00;}else if(n>=0xdc00&&n<=0xdfff)return false;utf8(out,unsigned(n));}
            else return false;
        }return false;
    }
    bool value(Value &out,int depth){
        space();if(depth>24 || ++nodes_>150000 || pos_==input_.size())return false;
        char c=input_[pos_];
        if(c=='"'){out.kind=Value::String;return string(out.text);}
        if(c=='{'||c=='['){
            const bool object=c=='{';out.kind=object?Value::Object:Value::Array;++pos_;if(take(object?'}':']'))return true;
            do{std::string key;if(object && (!string(key)||!take(':')))return false;Value child;if(!value(child,depth+1))return false;if(object){if(!out.fields.emplace(key,std::move(child)).second)return false;}else out.items.push_back(std::move(child));if(take(object?'}':']'))return true;}while(take(','));return false;
        }
        for(const auto &token:{std::string("true"),std::string("false"),std::string("null")})if(input_.compare(pos_,token.size(),token)==0){pos_+=token.size();out.kind=token=="null"?Value::Null:Value::Bool;out.text=token;return true;}
        const auto start=pos_;if(c=='-')++pos_;if(pos_==input_.size()||!std::isdigit(static_cast<unsigned char>(input_[pos_])))return false;
        if(input_[pos_]=='0')++pos_;else while(pos_<input_.size()&&std::isdigit(static_cast<unsigned char>(input_[pos_])))++pos_;
        if(pos_<input_.size()&&input_[pos_]=='.'){++pos_;auto digit=pos_;while(pos_<input_.size()&&std::isdigit(static_cast<unsigned char>(input_[pos_])))++pos_;if(digit==pos_)return false;}
        if(pos_<input_.size()&&(input_[pos_]=='e'||input_[pos_]=='E')){++pos_;if(pos_<input_.size()&&(input_[pos_]=='+'||input_[pos_]=='-'))++pos_;auto digit=pos_;while(pos_<input_.size()&&std::isdigit(static_cast<unsigned char>(input_[pos_])))++pos_;if(digit==pos_)return false;}
        out.kind=Value::Number;out.text=input_.substr(start,pos_-start);return true;
    }
public:
    explicit Parser(const std::string &input):input_(input){}
    bool parse(Value &out){if(input_.size()>4*1024*1024 || !value(out,0))return false;space();return pos_==input_.size();}
};
}
