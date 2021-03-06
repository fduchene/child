/*
Child, Internet Relay Chat Services
Copyright (C) 2005-2009  David Lebrun (target0@geeknode.org)

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.
    
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
    
You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/      

    
#include <child.h>
#include <globals.h>

/* parseline v2, greetz to wildcat for the idea */

int ParseLine(void)
{
    void *commands[] =
                {
                    "EOS",  m_eos,
                    "NICK", m_nick,
                    "KICK", m_kick,
                    "JOIN", m_join,
                    "QUIT", m_quit,
                    "KILL", m_kill,
                    "MODE", m_mode,
                    "PART", m_part,
                    "SETHOST",  m_sethost,
                    "SETIDENT", m_setident,
                    "CHGHOST",  m_chghost,
                    "CHGIDENT", m_chgident,
                    "PRIVMSG",  m_privmsg,
                    "TOPIC", m_topic,
                };
    void  *senders[] =
                {
                    "NICK", m_register_user,
                    "PING", m_ping,
                    "PROTOCTL", m_protoctl,
                    "TOPIC", m_stopic,
                };

    char *sender, *command, *tail;
    char *parv[3];

    sender = StripBlanks(indata.currentline);

    if (!sender || *sender == '\0')
        return 0;

#ifdef USE_FILTER
    if ((filter_check(sender, DIRECT_IN)) == RULE_DROP)
        return 0;
#endif

    command = SeperateWord(sender);
    tail = SeperateWord(command);

    parv[0] = sender;
    parv[1] = command;
    parv[2] = tail;

    if (vv) printf("%s %s %s\n",sender,command,tail);

    if (!command || *command == '\0')
        return 0;

    if (RunHooks(HOOK_RAW,NULL,NULL,NULL,parv) == MOD_STOP) return 0;

    unsigned int i;
    void (*func)();
    for (i=0;i<(sizeof(senders)/(sizeof(char *)))-1;i+=2) {
        if (!Strcmp(sender,senders[i])) {
            func = senders[i+1];
            func(command,tail);
            return 1;
        }   
    }

    for (i=0;i<(sizeof(commands)/(sizeof(char *)))-1;i+=2) {
        if (!Strcmp(command,commands[i])) {
            func = commands[i+1];
            func(sender,tail);
            return 1;
        }
    }

    return 1;
}

void m_chghost (char *sender __unused, char *tail)
{
    char *target,*newhost;
    Nick *nptr;

    target = tail;
    newhost = SeperateWord(target);
    SeperateWord(newhost);

    nptr = find_nick(target);
    if (!nptr) return;
    strncpy(nptr->hiddenhost,newhost,HOSTLEN);
}

void m_chgident (char *sender __unused, char *tail)
{
    char *target,*newident;
    Nick *nptr;

    target = tail;
    newident = SeperateWord(target);
    SeperateWord(newident);

    nptr = find_nick(target);
    if (!nptr) return;
    strncpy(nptr->ident,newident,NICKLEN);
}

void m_eos ()
{
    if (!eos)
	    joinallchans();
    eos = 1;
}

void m_join (char *sender, char *tail)
{
    Nick *nptr;
    User *uptr;
    char *chanjoined;
    Chan *chptr;
    char *bot;
    Cflag *member;
    TB *tb;
    char mask[256], mask2[256];

    sender++;
    chanjoined = tail;
    SeperateWord(chanjoined);

    if (!chanjoined || *chanjoined == '\0')
        return;

    nptr = find_nick(sender);
    if (!nptr)
        return;

    uptr = find_user(sender);

    char chans_copied[1024];
    bzero(chans_copied,1024);
    strncpy(chans_copied,chanjoined,1024);
    char *str_ptr;
    str_ptr = strtok(chans_copied,",");
    Wchan *wchan;
    while (str_ptr) {
        wchan = find_wchan(str_ptr);
        if (!wchan) wchan = CreateWchan(str_ptr);
        AddUserToWchan(nptr,wchan);

        chptr = find_channel(str_ptr);
        bot = whatbot(str_ptr);

        char *parv[1];
        parv[0] = str_ptr;
        if (RunHooks(HOOK_PRE_JOIN,nptr,uptr,chptr,parv) == MOD_STOP) { str_ptr = strtok(NULL,","); continue; }

        if (!chptr) {
            str_ptr = strtok(NULL,",");
            continue;
        }

        int hasaccess=0, flagged=0;
        member = find_cflag_r(nptr->nick, str_ptr);
        if (member) {
        if (uptr && uptr->authed == 1 && !HasOption(chptr, COPT_NOAUTO)) {
            if (HasOption(chptr, COPT_AXXFLAGS))
               sync_cflag(member, nptr);
            else {
            if (IsFounder(uptr,chptr)) {
                if (member->automode == CFLAG_AUTO_ON || member->automode == CFLAG_AUTO_OP) {
                    SetStatus(nptr,str_ptr,CHFL_OWNER|CHFL_OP,1,bot);
                    flagged = 1;
                }
                hasaccess = 1;
            } else if (GetFlag(uptr,chptr) >= me.chlev_admin) {
                if (member->automode == CFLAG_AUTO_ON || member->automode == CFLAG_AUTO_OP) {
                    SetStatus(nptr,str_ptr,CHFL_PROTECT|CHFL_OP,1,bot);
                    flagged = 1;
                }
                hasaccess = 1;
            } else if (GetFlag(uptr,chptr) >= me.chlev_op) {
                if (member->automode == CFLAG_AUTO_ON || member->automode == CFLAG_AUTO_OP) {
                    SetStatus(nptr,str_ptr,CHFL_OP,1,bot);
                    flagged = 1;
                }
                hasaccess = 1;
            } else if (GetFlag(uptr,chptr) >= me.chlev_halfop) {
                if (member->automode == CFLAG_AUTO_ON || member->automode == CFLAG_AUTO_OP) {
                    SetStatus(nptr,str_ptr,CHFL_HALFOP,1,bot);
                    flagged = 1;
                }
                hasaccess = 1;
            } else if (GetFlag(uptr,chptr) >= me.chlev_voice) {
                if (member->automode != CFLAG_AUTO_OFF) {
                    SetStatus(nptr,str_ptr,CHFL_VOICE,1,bot);
                    flagged = 1;
                }
                hasaccess = 1;
            } else if (GetFlag(uptr,chptr) == me.chlev_akick) {
                KickUser(bot,uptr->nick,str_ptr,"Get out of this chan !"); hasaccess = 1;
            } else if (GetFlag(uptr,chptr) == me.chlev_akb) {
                SendRaw(":%s MODE %s +b *!*@%s",bot,str_ptr,nptr->hiddenhost);
                KickUser(bot,uptr->nick,str_ptr,"Get out of this chan !"); hasaccess = 1;
            } else if (GetFlag(uptr,chptr) == me.chlev_nostatus)
                hasaccess = 1;

            if (hasaccess && !flagged && GetFlag(uptr,chptr) >= me.chlev_voice && member->automode != CFLAG_AUTO_OFF) {
                if (member->automode == CFLAG_AUTO_VOICE)
                    SetStatus(nptr,str_ptr,CHFL_VOICE,1,bot);
            }
            }
        }
        }

        if (!HasOption(chptr, COPT_AXXFLAGS)) {
        if (!HasOption(chptr, COPT_NOAUTO) && HasOption(chptr, COPT_ENABLEMASK) && !hasaccess) {
            Cflag *cflag;
            bzero(mask,256);
            bzero(mask2,256);
            snprintf(mask,256,"%s!%s@%s",nptr->nick,nptr->ident,nptr->hiddenhost);
            snprintf(mask2,256,"%s!%s@%s",nptr->nick,nptr->ident,nptr->host);
            LIST_FOREACH(cflag_list, cflag, HASH(str_ptr)) {
                if (!Strcmp(cflag->channel,str_ptr) && IsMask(cflag->nick) && (match_mask(cflag->nick,mask) || match_mask(cflag->nick,mask2))) {
                    if (cflag->flags == me.chlev_op && !HasOption(chptr, COPT_AOP))
                        SetStatus(nptr,str_ptr,CHFL_OP,1,bot);
                    else if (cflag->flags == me.chlev_halfop)
                        SetStatus(nptr,str_ptr,CHFL_HALFOP,1,bot);
                    else if (cflag->flags == me.chlev_voice && !HasOption(chptr, COPT_AVOICE))
                        SetStatus(nptr,str_ptr,CHFL_VOICE,1,bot);
                    else if (cflag->flags == me.chlev_akick)
                         KickUser(bot,nptr->nick,str_ptr,"Get out of this chan !");
                    else if (cflag->flags == me.chlev_akb) {
                        SendRaw(":%s MODE %s +b *!*@%s",bot,str_ptr,nptr->hiddenhost);
                        KickUser(bot,nptr->nick,str_ptr,"Get out of this chan !");
                    }
                }
            }
        }
        }

        if (uptr && uptr->authed == 1) {
            if (IsFounder(uptr,chptr) || GetFlag(uptr,chptr) > 0)
                chptr->lastseen = time(NULL);
        }

        if (!HasOption(chptr, COPT_NOAUTO) && !hasaccess) {
            if ((uptr && uptr->authed == 1 && HasOption(chptr, COPT_SECURE)) || (!HasOption(chptr, COPT_SECURE))) {
                if (HasOption(chptr, COPT_AVOICE))
                    SetStatus(nptr,str_ptr,CHFL_VOICE,1,bot);
                if (HasOption(chptr, COPT_AOP))
                    SetStatus(nptr,str_ptr,CHFL_OP,1,bot);
            }
        }

        if (chptr->entrymsg != NULL && chptr->entrymsg[0] != '\0')
            FakeNotice(bot,nptr,"[%s] %s",chptr->channelname,chptr->entrymsg);

/*        if (members_num(wchan) == 1)
            JoinChannel(whatbot(wchan->chname),wchan->chname);*/

        RunHooks(HOOK_JOIN,nptr,uptr,chptr,NULL);

        bzero(mask,256);
        bzero(mask2,256);
        snprintf(mask, 256, "%s!%s@%s", nptr->nick, nptr->ident, nptr->hiddenhost);
        snprintf(mask2, 256, "%s!%s@%s", nptr->nick, nptr->ident, nptr->host);
        LIST_FOREACH(tb_list, tb, HASH(str_ptr)) {
            if (!Strcmp(tb->channel, str_ptr) && (match_mask(tb->mask, mask) || match_mask(tb->mask, mask2))) {
                SendRaw(":%s MODE %s +b *!*@%s", bot, str_ptr, nptr->hiddenhost);
                KickUser(bot, nptr->nick, str_ptr, "%s", tb->reason);
                break;
            }
        }

        str_ptr = strtok(NULL,",");
    }
}

void m_kick (char *sender, char *tail)
{
    char *chan;
    char *nick;

    chan = tail;
    nick = SeperateWord(chan);
    SeperateWord(nick);

    sender++;

    Nick *nptr;
    Wchan *wchan;

    wchan = find_wchan(chan);
    if (!wchan) return;

    char *bot = whatbot(chan);

    if (!Strcmp(nick,bot)) {
        JoinChannel(bot,chan);
        KickUser(bot,sender,chan,"are you mad ?");
        nptr = find_nick(sender);
        if (!nptr) return;
        DeleteUserFromWchan(nptr,wchan);
        return;
    }

    nptr = find_nick(nick);
    if (!nptr) return;

    DeleteUserFromWchan(nptr,wchan);
}

void m_kill (char *sender, char *tail)
{
    char *nick;
    Nick *nptr;
    User *uptr;
    Bot *bot;
    Chanbot *chanbot;

    sender++;
    nick = tail;
    SeperateWord(nick);

    if (!nick || *nick == '\0')
        return;

    nptr = find_nick(nick);

    if (!nptr) {
        if (!Strcmp(nick, me.nick)) {
            fakeuser(me.nick,me.ident,me.host,MY_UMODES);
            Chan *chptr;
            LIST_FOREACH_ALL(chan_list, chptr) {
                if (!HasOption(chptr, COPT_NOJOIN))
                    JoinChannel(me.nick,chptr->channelname);
            }
            killuser(sender,"That is something not recommended...",me.nick);
            return;
        }

        LIST_FOREACH_ALL(bot_list, bot) {
            if (!Strcmp(nick,bot->nick)) {
                fakeuser(bot->nick,bot->ident,bot->host,BOTSERV_UMODES);
                LIST_FOREACH_ALL(chanbot_list, chanbot) {
                    if (!Strcmp(chanbot->bot,bot->nick))
                        JoinChannel(bot->nick,chanbot->name);
                }
                killuser(sender,"That is something not recommended...",bot->nick);
                return;
            }
        }
    }

    uptr = find_user(nick);
    if (RunHooks(HOOK_KILL,nptr,uptr,NULL,NULL) == MOD_STOP)
        return;

    if (!nptr) return;

    userquit(nick);
}

void m_mode (char *sender, char *tail)
{
    char *nick,*chan;
    char *umode,*modes;
    Nick *nptr,*nptr2=NULL;
    char *parv[1];
    char *args[50];
    User *uptr = NULL;
    Member *member = NULL;
    Wchan *wchan;
    Chan *channel;
    int i=0;

    if (strstr(tail,":")) {
        nick = tail;
        umode = SeperateWord(nick);
        SeperateWord(umode);

        if (!nick || !umode || *nick == '\0' || *umode == '\0')
            return;

        if (nick[0] == '#')
            return;

        nptr = find_nick(nick);
        if (!nptr)
            return;

        uptr = find_user(nick);
        umode++;
        if (umode[0] == '+') {
            if (IsCharInString('o',umode)) {
                SetOper(nptr);
                globops("\2%s\2 is now an IRC Operator",nptr->nick);
            }
            if (IsCharInString('a',umode)) SetSAdmin(nptr);
            if (IsCharInString('A',umode)) SetAdmin(nptr);
            if (IsCharInString('N',umode)) SetNAdmin(nptr);
            if (IsCharInString('B',umode)) SetBot(nptr);
            if (IsCharInString('S',umode)) SetService(nptr);
            if (IsCharInString('q',umode)) SetNokick(nptr);
        } else if (umode[0] == '-') {
            if (IsCharInString('o',umode)) ClearOper(nptr);
            if (IsCharInString('a',umode)) ClearSAdmin(nptr);
            if (IsCharInString('A',umode)) ClearAdmin(nptr);
            if (IsCharInString('N',umode)) ClearNAdmin(nptr);
            if (IsCharInString('B',umode)) ClearBot(nptr);
            if (IsCharInString('S',umode)) ClearService(nptr);
            if (IsCharInString('q',umode)) ClearNokick(nptr);
            if (IsCharInString('x',umode)) strncpy(nptr->hiddenhost,nptr->host,HOSTLEN);
        }

        parv[0] = umode;
        RunHooks(HOOK_UMODE,nptr,uptr,NULL,parv);
        return;
    } else {
        chan = tail;
        modes = SeperateWord(chan);
        if (!modes || *modes == '\0') return;

        args[0] = SeperateWord(modes);
        while ((args[i+1] = SeperateWord(args[i]))) i++;

        sender++;

        char *bot;
        char modesbis[33];
        char *modesm,*modesm2,*modesm3 = NULL,*modesm4,*modesm5;
        char modesbis2[33];
        strncpy(modesbis,modes,32);

        wchan = find_wchan(chan);
        if (!wchan) return;
        channel = find_channel(chan);

        /* Let's check for mlocks */

        if (channel) {
            bot = whatbot(channel->channelname);
            unsigned int k;
            if (channel->mlock[0] != '\0') {
                strncpy(modesbis2,channel->mlock,32);
                switch(*modesbis) {
                    case '+':
                        modesm = strstr(modesbis,"-");
                        if (modesm) *modesm = '\0';

                        modesm2 = strstr(modesbis2,"-");
                        if (!modesm2) goto nextp;
                        modesm2++;

                        modesm3 = strstr(modesm2,"+");
                        if (modesm3) *modesm3 = '\0';

                        for (k=0;k<strlen(modesbis);k++) {
                            if (IsCharInString(*(modesbis+k),modesm2))
                                SendRaw(":%s MODE %s %s",bot,channel->channelname,channel->mlock);
                        }

                        nextp:
                        if (modesm) modesm++;
                        if (!modesm || *modesm == '\0') break;

                        if (modesm3 && *modesm3 == '\0') *modesm3 = '+';

                        modesm4 = strstr(modesbis2,"+");
                        if (!modesm4) break;
                        modesm4++;

                        modesm5 = strstr(modesm4,"-");
                        if (modesm5) *modesm5 = '\0';

                        for (k=0;k<strlen(modesm);k++) {
                            if (IsCharInString(*(modesm+k),modesm4))
                                SendRaw(":%s MODE %s %s",bot,channel->channelname,channel->mlock);
                        }

                        break;
                    case '-':
                        modesm = strstr(modesbis,"+");
                        if (modesm) *modesm = '\0';

                        modesm2 = strstr(modesbis2,"+");
                        if (!modesm2) goto nextm;
                        modesm2++;

                        modesm3 = strstr(modesm2,"-");
                        if (modesm3) *modesm3 = '\0';

                        for (k=0;k<strlen(modesbis);k++) {
                            if (IsCharInString(*(modesbis+k),modesm2))
                                SendRaw(":%s MODE %s %s",bot,channel->channelname,channel->mlock);
                        }

                        nextm:
                        if (modesm) modesm++;
                        if (!modesm || *modesm == '\0') break;

                        if (modesm3 && *modesm3 == '\0') *modesm3 = '-';

                        modesm4 = strstr(modesbis2,"-");
                        if (!modesm4) break;
                        modesm4++;

                        modesm5 = strstr(modesm4,"+");
                        if (modesm5) *modesm5 = '\0';

                        for (k=0;k<strlen(modesm);k++) {
                            if (IsCharInString(*(modesm+k),modesm4))
                                SendRaw(":%s MODE %s %s",bot,channel->channelname,channel->mlock);
                        }

                        break;
                }
            }
        }

        /* End of mlocks checking */

        bot = whatbot(chan);

        Chanbot *chanbot;
        int warg=0,len;
        switch(*modes) {
            case '+':
                cplus:
                modes++;
                len = strlen(modes);
                for (i=0;*modes && i<=len; i++,modes++) {
                    if (*modes == 'q' || *modes == 'a' || *modes == 'o' || *modes == 'h' || *modes == 'v') {
                        nptr2 = find_nick(args[warg]);
                        if (!nptr2) { warg++; continue; }
                        member = find_member(wchan->chname,args[warg]);
                        if (!member) { warg++; continue; }
                        uptr = find_user(args[warg]);
                        if (GetFlag(uptr,channel) == me.chlev_nostatus && IsAuthed(uptr)) {
                            int w = 0;
                            switch (*modes) {
                                case 'q':
                                    w = CHFL_OWNER;
                                    break;
                                case 'a':
                                    w = CHFL_PROTECT;
                                    break;
                                case 'o':
                                    w = CHFL_OP;
                                    break;
                                case 'h':
                                    w = CHFL_HALFOP;
                                    break;
                                case 'v':
                                    w = CHFL_VOICE;
                                    break;
                            }
                            SetStatus(nptr2,chan,w,0,bot);
                            warg++;
                            continue;
                        }
                    }

                    switch(*modes) {
                        case 'q':
                            if (channel) {
                                if ((!IsFounder(uptr,channel) || !IsAuthed(uptr)) && HasOption(channel, COPT_STRICTOP)) {
                                    SetStatus(nptr2,chan,CHFL_OWNER,0,bot);
                                    warg++;
                                    break;
                                }
                            }

                            SetOwner(member);
                            warg++;
                            break;
                        case 'a':
                            if (channel) {
                                if ((GetFlag(uptr,channel) < me.chlev_admin || !IsAuthed(uptr)) && HasOption(channel, COPT_STRICTOP)) {
                                    SetStatus(nptr2,chan,CHFL_PROTECT,0,bot);
                                    warg++;
                                    break;
                                }
                            }

                            SetProtect(member);
                            warg++;
                            break;
                        case 'o':
                            if (channel) {
                                if ((members_num(wchan) == 1 && (GetFlag(uptr,channel) < me.chlev_op || !IsAuthed(uptr))) ||
                                    ((GetFlag(uptr,channel) < me.chlev_op || !IsAuthed(uptr)) &&
                                    channel->options & HasOption(channel, COPT_STRICTOP))) {
                                    SetStatus(nptr2,chan,CHFL_OP,0,bot);
                                    warg++;
                                    break;
                                }
                            }

                            SetOp(member);
                            warg++;
                            break;
                        case 'h':
                            if (channel) {
                                if ((GetFlag(uptr,channel) < me.chlev_halfop || !IsAuthed(uptr)) && HasOption(channel, COPT_STRICTOP)) {
                                    SetStatus(nptr2,chan,CHFL_HALFOP,0,bot);
                                    warg++;
                                    break;
                                }
                            }

                            SetHalfop(member);
                            warg++;
                            break;
                        case 'v':
                            SetVoice(member);
                            warg++;
                            break;
                        case '-':
                            goto cmin;
                        case 'b':
                        case 'e':
                        case 'I':
                        case 'f':
                        case 'j':
                        case 'k':
                        case 'l':
                        case 'L':
                            warg++;
                            break;
                    }
                }
                break;
            case '-':
                cmin:
                modes++;
                len = strlen(modes);
                for (i=0; *modes && i<=len; i++,modes++) {
                    if (*modes == 'q' || *modes == 'a' || *modes == 'o' || *modes == 'h' || *modes == 'v') {
                        nptr2 = find_nick(args[warg]);
                        member = find_member(wchan->chname,args[warg]);
                        uptr = find_user(args[warg]);
                    }

                    switch(*modes) {
                        case 'q':
                            if (!member) { warg++; break; }
                            if (channel) {
                                if (Strcmp(uptr->nick, sender) && IsFounder(uptr,channel) && IsAuthed(uptr) && HasOption(channel, COPT_PROTECTOPS)) {
                                    SetStatus(nptr2,chan,CHFL_OWNER,1,bot);
                                    warg++;
                                    break;
                                }
                            }

                            ClearOwner(member);
                            warg++;
                            break;
                        case 'a':
                            if (!Strcmp(args[warg],me.nick)) {
                                SendRaw("MODE %s +a %s",chan,bot);
                                warg++;
                                break;
                            }
                            if ((chanbot = find_chanbot(chan)) != NULL) {
                                if (!Strcmp(args[warg],chanbot->bot)) {
                                    SendRaw("MODE %s +a %s",chan,chanbot->bot);
                                    warg++;
                                    break;
                                }
                            }
                            if (!member) { warg++; break; }
                            if (channel) {
                                if (Strcmp(uptr->nick, sender) && GetFlag(uptr,channel) >= me.chlev_admin && IsAuthed(uptr) && HasOption(channel, COPT_PROTECTOPS)) {
                                    SetStatus(nptr2,chan,CHFL_PROTECT,1,bot);
                                    warg++;
                                    break;
                                }
                            }

                            ClearProtect(member);
                            warg++;
                            break;
                        case 'o':
                            if (!Strcmp(args[warg],me.nick)) {
                                SendRaw("MODE %s +o %s",chan,bot);
                                warg++;
                                break;
                            }
                            if ((chanbot = find_chanbot(chan)) != NULL) {
                                if (!Strcmp(args[warg],chanbot->bot)) {
                                    SendRaw("MODE %s +o %s",chan,chanbot->bot);
                                    warg++;
                                    break;
                                }
                            }
                            if (!member) { warg++; break; }
                            if (channel) {
                                if (Strcmp(uptr->nick, sender) && GetFlag(uptr,channel) >= me.chlev_op && IsAuthed(uptr) && HasOption(channel, COPT_PROTECTOPS)) {
                                    SetStatus(nptr2,chan,CHFL_OP,1,bot);
                                    warg++;
                                    break;
                                }
                            }

                            ClearOp(member);
                            warg++;
                            break;
                        case 'h':
                            if (!member) { warg++; break; }
                            ClearHalfop(member);
                            warg++;
                            break;
                        case 'v':
                            if (!member) { warg++; break; }
                            ClearVoice(member);
                            warg++;
                            break;
                        case '+':
                            goto cplus;
                        case 'b':
                        case 'e':
                        case 'I':
                        case 'f':
                        case 'j':
                        case 'k':
                        case 'l':
                        case 'L':
                            warg++;
                            break;
                    }
                }
                break;
        }
    }
}

void m_nick (char *sender, char *tail)
{
    char *newnick;
    Nick *nptr;
    User *uptr,*uptr2 = NULL;
    char *parv[1];

    sender++;

    newnick = tail;
    SeperateWord(newnick);

    nptr = find_nick(sender);
    if (!nptr)
        return;

    DeleteGuest(nptr->nick);

    strncpy(nptr->nick,newnick,NICKLEN - 1);
    nptr->nick[NICKLEN - 1] = '\0';
    LIST_REMOVE(nick_list, nptr, HASH(sender));
    LIST_INSERT_HEAD(nick_list, nptr, HASH(newnick));

    uptr = find_user(sender);

    Member *member;
    LIST_FOREACH_ALL(member_list, member) {
        if (!Strcmp(member->nick,sender))
            strncpy(member->nick,newnick,NICKLEN - 1);
    }

    if (!Strcmp(newnick,sender)) return;

    uptr2 = find_user(newnick);
    if (uptr2) {
        if (uptr && (find_link2(sender,newnick) || find_link2(newnick,sender)) && uptr->authed == 1) {
            uptr->authed = 0;
            uptr->lastseen = time(NULL);
            uptr2->authed = 1;
            SendRaw("SVSMODE %s +r",newnick);
            uptr2->lastseen = time(NULL);
        } else {
            if (uptr && uptr->authed == 1)
                SendRaw("SVSMODE %s -r",newnick);
            NoticeToUser(nptr,"This nick is registered. Please identify yourself or take another nick.");
            if (HasOption(uptr2,UOPT_PROTECT)) AddGuest(newnick,uptr2->timeout,time(NULL));
        }
    }

    if (uptr) {
        uptr->authed = 0;
        uptr->lastseen = time(NULL);
    }

    nptr = find_nick(newnick);
    parv[0] = sender;

    RunHooks(HOOK_NICKCHANGE,nptr,uptr2,NULL,parv);
}

void m_part (char *sender, char *tail)
{
    char *chan;
    Nick *nptr;
    Wchan *wchan;

    sender++;
    chan = tail;
    SeperateWord(chan);

    nptr = find_nick(sender);
    if (!nptr) return;
    wchan = find_wchan(chan);
    if (!wchan) return;
    DeleteUserFromWchan(nptr,wchan);
/*    if (!member_exists(wchan) && find_channel(wchan->chname))
        PartChannel(wchan->chname);*/
}

void m_ping (char *command)
{
    SendRaw("PONG %s",command+1);
    RunHooks(HOOK_PING,NULL,NULL,NULL,NULL);
}

void m_protoctl ()
{
    loadallfakes();
    RunHooks(HOOK_CONNECTED,NULL,NULL,NULL,NULL);
}

void m_privmsg (char *sender, char *tail)
{
    char *target,*ch_ptr;
    char gnuh[1024];
    Nick *nptr;
    User *uptr;
    Wchan *wchan=NULL;
    Chan *chptr=NULL;
    char *args[2];

    if (!tail || *tail == '\0')
        return;

    ch_ptr = strchr(tail,':');
    if (!ch_ptr)
        return;
    ch_ptr++;

    sender++;
    target = tail;
    SeperateWord(target);

    strncpy(gnuh,ch_ptr,1024-1);
    gnuh[1024-1] = '\0';

    ch_ptr = StripBlanks(ch_ptr);
    char tmp_[1024];
    strncpy(tmp_,ch_ptr,1024);
    char *arg1;
    arg1 = tmp_;
    SeperateWord(arg1);

    if (!ch_ptr || *ch_ptr == '\0')
        return;

    nptr = find_nick(sender);
    if (!nptr) return;

    if (target[0] != '#' && !IsOper(nptr)) {
        if (nptr->ignored) {
            if (time(NULL) - nptr->ignoretime > me.ignoretime) {
                nptr->ignored = 0;
                nptr->ignoretime = 0;
            } else
                return;
        } else if (nptr->msgtime == 0 || nptr->msgnb == 0) {
            nptr->msgtime = time(NULL);
            nptr->msgnb = 1;
        } else if (time(NULL) - nptr->msgtime <= me.maxmsgtime && nptr->msgnb >= me.maxmsgnb) {
            nptr->ignored = 1;
            nptr->ignoretime = time(NULL);
            return;
        } else if (time(NULL) - nptr->msgtime > me.maxmsgtime && nptr->msgnb <= me.maxmsgnb) {
            nptr->msgtime = time(NULL);
            nptr->msgnb = 1;
        } else
            nptr->msgnb++;
    }

    if (target[0] == '#') {
        chptr = find_channel(target);
        wchan = find_wchan(target);
    }

    uptr = find_user(sender);

    args[0] = target;
    char parv_tab[1024];
    args[1] = parv_tab;
    bzero(args[1],1024-1);
    strncpy(args[1],gnuh,1024-1);
    if (RunHooks(HOOK_PRE_PRIVMSG,nptr,uptr,chptr,args) == MOD_STOP)
        return;

    Command *cmd;

    if (target[0] == '#') {
        if (!chptr || !wchan) return;
        ch_ptr = SeperateWord(ch_ptr);
        LIST_FOREACH(command_list, cmd, HASH_INT(CMD_BOT)) {
            if (!Strcmp(cmd->name,arg1) && cmd->type == CMD_BOT) {
                if ((!IsAuthed(uptr) && cmd->level == 0) || (IsAuthed(uptr) && uptr->level >= cmd->level))
                    cmd->func(nptr,uptr,chptr,wchan,ch_ptr);
                return;
            }
        }
        return;
    }

    char mymask[256];
    snprintf(mymask,256,"%s@%s",me.nick,me.name);

    if (Strcmp(target,me.nick) && Strcmp(target,mymask))
        return;

    if (!Strcmp(arg1,"\1VERSION\1")) {
        NoticeToUser(nptr,"\1VERSION Child v%s written by target0, compiled on %s at %s\1",VERSION,__DATE__,__TIME__);
        return;
    }

    LIST_FOREACH(command_list, cmd, HASH_INT(CMD_BASE)) {
        if (!Strcmp(cmd->name,arg1) && cmd->type == CMD_BASE) {
            if ((!IsAuthed(uptr) && cmd->level == 0) || (IsAuthed(uptr) && uptr->level >= cmd->level))
                cmd->func(nptr,uptr,ch_ptr);
            else
                NoticeToUser(nptr,"Access denied");
            return;
        }
    }

    if (RunHooks(HOOK_PRIVMSG,nptr,uptr,chptr,args) == MOD_STOP)
        return;

    NoticeToUser(nptr,"Unknown command");
}

void m_quit (char *sender)
{
    Nick *nptr;
    User *uptr;

    sender++;
    nptr = find_nick(sender);
    if (!nptr)
        return;

    uptr = find_user(sender);

    if (RunHooks(HOOK_QUIT,nptr,uptr,NULL,NULL) == MOD_STOP)
        return;

    userquit(sender);
}

void m_register_user (char *command, char *tail)
{
    char *nick;
    char *umode;
    char *ident;
    char *host;
    char *server;
    char *hiddenhost;
    char *nickip;
    long int modes=0;
    Nick *nptr;

    nick = command;
    ident = tail;
    ident = SeperateWord(ident);
    ident = SeperateWord(ident);
    host = SeperateWord(ident);
    server = SeperateWord(host);
    umode = SeperateWord(server);
    umode = SeperateWord(umode);
    hiddenhost = SeperateWord(umode);
    nickip = SeperateWord(hiddenhost);
    SeperateWord(nickip);

    if (!nick || !ident || !host || !umode || *nick == '\0' || *ident == '\0' || *host == '\0'
        || *umode == '\0' || !server || *server == '\0'|| !hiddenhost || *hiddenhost == '\0'
        || !nickip || *nickip == '\0') {
        return;
    }

    char clientip[HOSTLEN+1];
    bzero(clientip,HOSTLEN);
    if (*nickip == '*')
        strncpy(clientip,host,HOSTLEN);
    else
        strncpy(clientip,decode_ip(nickip),HOSTLEN);

    Trust *trust;
    int clones;
    trust = find_trust_strict(host);
    if (!trust)
        trust = find_trust(clientip);
    clones = howmanyclones(clientip);
    if (trust) {
        if (clones >= trust->limit) {
            _killuser(nick,"Max clones limit exceeded",me.nick);
            return;
        }
    } else {
        if (clones >= me.maxclones) {
            _killuser(nick,"Max clones limit exceeded",me.nick);
            return;
        }
    }

    if (IsCharInString('o',umode)) modes |= UMODE_OPER;
    if (IsCharInString('a',umode)) modes |= UMODE_SADMIN;
    if (IsCharInString('A',umode)) modes |= UMODE_ADMIN;
    if (IsCharInString('r',umode)) modes |= UMODE_REGISTERED;
    if (IsCharInString('N',umode)) modes |= UMODE_NADMIN;
    if (IsCharInString('B',umode)) modes |= UMODE_BOT;
    if (IsCharInString('S',umode)) modes |= UMODE_SERVICE;
    if (IsCharInString('q',umode)) modes |= UMODE_NOKICK;
    if (IsCharInString('z',umode)) modes |= UMODE_SSL;

    nptr = AddNick(nick,ident,host,server,hiddenhost,modes,clientip);

    User *uptr;
    uptr = find_user(nptr->nick);
    if (uptr) {
        if (IsRegistered(nptr))
            uptr->authed = 1;
        else {
            uptr->authed = 0;
            if (!IsUserSuspended(uptr)) {
                NoticeToUser(nptr,"This nick is registered. Please identify yourself or take another nick.");
                if (uptr->options & UOPT_PROTECT)
                    AddGuest(nptr->nick,uptr->timeout,time(NULL));
            }
        }
    }

    RunHooks(HOOK_NICKCREATE,nptr,uptr,NULL,NULL);
    return;
}

void m_sethost (char *sender, char *tail)
{
    char *newhost;
    Nick *nptr;

    sender++;
    newhost = tail;
    SeperateWord(newhost);

    nptr = find_nick(sender);
    if (!nptr) return;
    strncpy(nptr->hiddenhost,newhost,HOSTLEN);
}

void m_setident (char *sender, char *tail)
{
    char *newident;
    Nick *nptr;

    sender++;
    newident = tail;
    SeperateWord(newident);

    nptr = find_nick(sender);
    if (!nptr) return;
    strncpy(nptr->ident,newident,NICKLEN);
}

void m_topic (char *sender, char *tail)
{
    Chan *chptr;
    char *topic, *chan;

    sender++;
    chan = tail;
    topic = SeperateWord(chan);
    if ((topic = strchr(topic, ':')) == NULL)
        return;

    topic++;
    if ((chptr = find_channel(chan)) == NULL)
        return;

    if (chptr->options & COPT_ENFTOPIC)
        SendRaw(":%s TOPIC %s :%s", whatbot(chan), chan, chptr->topic);
    else
        strncpy(chptr->topic, topic, TOPICLEN);
}

void m_stopic (char *command, char *tail)
{
    Wchan *wchan;
    Chan *chptr;
    char *chname, *setter, *ts, *topic;
    chname = command;
    setter = tail;
    ts = SeperateWord(tail);
    if ((topic = strchr(ts, ':')) != NULL)
        topic++;

    wchan = find_wchan(chname); /* assuming not NULL, debugging purpose */
    strncpy(wchan->topic, topic, TOPICLEN);
    if ((chptr = find_channel(chname)) != NULL) {
        if (chptr->topic[0] == '\0')
            strncpy(chptr->topic, topic, TOPICLEN);
    }
}
