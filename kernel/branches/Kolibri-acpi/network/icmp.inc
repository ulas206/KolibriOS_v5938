;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;                                                                 ;;
;; Copyright (C) KolibriOS team 2004-2015. All rights reserved.    ;;
;; Distributed under terms of the GNU General Public License       ;;
;;                                                                 ;;
;;  ICMP.INC                                                       ;;
;;                                                                 ;;
;;  Part of the tcp/ip network stack for KolibriOS                 ;;
;;                                                                 ;;
;;  Based on the work of [Johnny_B] and [smb]                      ;;
;;                                                                 ;;
;;    Written by hidnplayr@kolibrios.org                           ;;
;;                                                                 ;;
;;          GNU GENERAL PUBLIC LICENSE                             ;;
;;             Version 2, June 1991                                ;;
;;                                                                 ;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

$Revision: 5594 $

; ICMP types & codes

ICMP_ECHOREPLY                  = 0               ; echo reply message

ICMP_UNREACH                    = 3
ICMP_UNREACH_NET                = 0               ; bad net
ICMP_UNREACH_HOST               = 1               ; bad host
ICMP_UNREACH_PROTOCOL           = 2               ; bad protocol
ICMP_UNREACH_PORT               = 3               ; bad port
ICMP_UNREACH_NEEDFRAG           = 4               ; IP_DF caused drop
ICMP_UNREACH_SRCFAIL            = 5               ; src route failed
ICMP_UNREACH_NET_UNKNOWN        = 6               ; unknown net
ICMP_UNREACH_HOST_UNKNOWN       = 7               ; unknown host
ICMP_UNREACH_ISOLATED           = 8               ; src host isolated
ICMP_UNREACH_NET_PROHIB         = 9               ; prohibited access
ICMP_UNREACH_HOST_PROHIB        = 10              ; ditto
ICMP_UNREACH_TOSNET             = 11              ; bad tos for net
ICMP_UNREACH_TOSHOST            = 12              ; bad tos for host
ICMP_UNREACH_FILTER_PROHIB      = 13              ; admin prohib
ICMP_UNREACH_HOST_PRECEDENCE    = 14              ; host prec vio.
ICMP_UNREACH_PRECEDENCE_CUTOFF  = 15              ; prec cutoff

ICMP_SOURCEQUENCH               = 4               ; Packet lost, slow down

ICMP_REDIRECT                   = 5               ; shorter route, codes:
ICMP_REDIRECT_NET               = 0               ; for network
ICMP_REDIRECT_HOST              = 1               ; for host
ICMP_REDIRECT_TOSNET            = 2               ; for tos and net
ICMP_REDIRECT_TOSHOST           = 3               ; for tos and host

ICMP_ALTHOSTADDR                = 6               ; alternate host address
ICMP_ECHO                       = 8               ; echo service
ICMP_ROUTERADVERT               = 9               ; router advertisement
ICMP_ROUTERADVERT_NORMAL        = 0                  ; normal advertisement
ICMP_ROUTERADVERT_NOROUTE_COMMON= 16         ; selective routing

ICMP_ROUTERSOLICIT              = 10              ; router solicitation
ICMP_TIMXCEED                   = 11              ; time exceeded, code:
ICMP_TIMXCEED_INTRANS           = 0               ; ttl==0 in transit
ICMP_TIMXCEED_REASS             = 1               ; ttl==0 in reass

ICMP_PARAMPROB                  = 12               ; ip header bad
ICMP_PARAMPROB_ERRATPTR         = 0            ; error at param ptr
ICMP_PARAMPROB_OPTABSENT        = 1            ; req. opt. absent
ICMP_PARAMPROB_LENGTH           = 2            ; bad length

ICMP_TSTAMP                     = 13              ; timestamp request
ICMP_TSTAMPREPLY                = 14              ; timestamp reply
ICMP_IREQ                       = 15              ; information request
ICMP_IREQREPLY                  = 16              ; information reply
ICMP_MASKREQ                    = 17              ; address mask request
ICMP_MASKREPLY                  = 18              ; address mask reply
ICMP_TRACEROUTE                 = 30              ; traceroute
ICMP_DATACONVERR                = 31              ; data conversion error
ICMP_MOBILE_REDIRECT            = 32              ; mobile host redirect
ICMP_IPV6_WHEREAREYOU           = 33              ; IPv6 where-are-you
ICMP_IPV6_IAMHERE               = 34              ; IPv6 i-am-here
ICMP_MOBILE_REGREQUEST          = 35              ; mobile registration req
ICMP_MOBILE_REGREPLY            = 36              ; mobile registreation reply
ICMP_SKIP                       = 39              ; SKIP

ICMP_PHOTURIS                   = 40              ; Photuris
ICMP_PHOTURIS_UNKNOWN_INDEX     = 1                ; unknown sec index
ICMP_PHOTURIS_AUTH_FAILED       = 2                ; auth failed
ICMP_PHOTURIS_DECRYPT_FAILED    = 3                ; decrypt failed



struct  ICMP_header

        Type                    db ?
        Code                    db ?
        Checksum                dw ?
        Identifier              dw ?
        SequenceNumber          dw ?

ends


uglobal
align 4

        ICMP_PACKETS_TX         rd NET_DEVICES_MAX
        ICMP_PACKETS_RX         rd NET_DEVICES_MAX

endg



;-----------------------------------------------------------------
;
; ICMP_init
;
;-----------------------------------------------------------------

macro ICMP_init {

        xor     eax, eax
        mov     edi, ICMP_PACKETS_TX
        mov     ecx, 2*NET_DEVICES_MAX
        rep stosd

}


;-----------------------------------------------------------------
;
; ICMP_input:
;
;  This procedure will send reply's to ICMP echo's
;   and insert packets into sockets when needed
;
;  IN:  Pointer to buffer in [esp]
;       ebx = pointer to device struct
;       ecx = ICMP Packet size
;       esi = ptr to ICMP Packet data
;       edi = ptr to ipv4 source and dest address
;
;  OUT: /
;
;-----------------------------------------------------------------
align 4
ICMP_input:

        DEBUGF  DEBUG_NETWORK_VERBOSE, "ICMP_input\n"

; Check the checksum
        push    esi ecx
        push    [esi + ICMP_header.Checksum]
        mov     [esi + ICMP_header.Checksum], 0
        xor     edx, edx
        call    checksum_1
        call    checksum_2
        pop     si
        cmp     dx, si
        pop     ecx esi
        jne     .checksum_mismatch

        DEBUGF  DEBUG_NETWORK_VERBOSE, "ICMP_input: Checksum OK\n"

; Ualidate device ptr
        mov     eax, edi
        call    NET_ptr_to_num4
        cmp     edi, -1
        je      .dump

; Update stats
        inc     [ICMP_PACKETS_RX + edi]

; Is this an echo request?
        cmp     [esi + ICMP_header.Type], ICMP_ECHO
        je      .echo_request

; Look for an open ICMP socket
        pusha
        mov     ecx, socket_mutex
        call    mutex_lock
        popa

        mov     edx, [eax]              ; ipv4 source address
        mov     eax, net_sockets
  .try_more:
;        mov      , [esi + ICMP_header.Identifier]
  .next_socket:
        mov     eax, [eax + SOCKET.NextPtr]
        or      eax, eax
        jz      .dump_

        cmp     [eax + SOCKET.Domain], AF_INET4
        jne     .next_socket

        cmp     [eax + SOCKET.Protocol], IP_PROTO_ICMP
        jne     .next_socket

        cmp     [eax + IP_SOCKET.RemoteIP], edx
        jne     .next_socket

;        cmp     [eax + ICMP_SOCKET.Identifier],
;        jne     .next_socket

        pusha
        mov     ecx, socket_mutex
        call    mutex_unlock
        popa

        DEBUGF  DEBUG_NETWORK_VERBOSE, "socket=%x\n", eax

        pusha
        lea     ecx, [eax + SOCKET.mutex]
        call    mutex_lock
        popa

        jmp     SOCKET_input



  .echo_request:

; We'll reuse the packet so we can create the response as fast as possible
        DEBUGF  DEBUG_NETWORK_VERBOSE, "ICMP echo request\n"

; Change Packet type to reply
        mov     [esi + ICMP_header.Type], ICMP_ECHOREPLY

        mov     eax, [esp]
        lea     esi, [eax + NET_BUFF.data]

; Check frame type
        cmp     [eax + NET_BUFF.type], NET_BUFF_ETH
        jne     .not_ethernet

; exchange dest and source MAC in ETH header
        push    dword [esi + ETH_header.DstMAC]
        push    dword [esi + ETH_header.SrcMAC]
        pop     dword [esi + ETH_header.DstMAC]
        pop     dword [esi + ETH_header.SrcMAC]
        push    word [esi + ETH_header.DstMAC + 4]
        push    word [esi + ETH_header.SrcMAC + 4]
        pop     word [esi + ETH_header.DstMAC + 4]
        pop     word [esi + ETH_header.SrcMAC + 4]
        add     esi, sizeof.ETH_header

  .not_ethernet:
; Exchange dest and source address in IP header
        push    [esi + IPv4_header.SourceAddress]
        push    [esi + IPv4_header.DestinationAddress]
        pop     [esi + IPv4_header.SourceAddress]
        pop     [esi + IPv4_header.DestinationAddress]

; Calculate IP header length
        movzx   ecx, [esi + IPv4_header.VersionAndIHL]
        and     ecx, 0x0f
        shl     cx, 2
        mov     edi, ecx                        ; put it in edi for later

; Calculate IP checksum
        mov     eax, esi
        mov     [eax + IPv4_header.HeaderChecksum], 0
        xor     edx, edx
        call    checksum_1
        call    checksum_2
        mov     [eax + IPv4_header.HeaderChecksum], dx

; Calculate ICMP packet length
        movzx   ecx, [eax + IPv4_header.TotalLength]
        xchg    ch, cl
        sub     ecx, edi                        ; IP packet length - IP header length = ICMP packet length

; Calculate ICMP checkSum
        mov     eax, esi
        mov     [esi + ICMP_header.Checksum], 0
        xor     edx, edx
        call    checksum_1
        call    checksum_2
        mov     [eax + ICMP_header.Checksum], dx

; Transmit the frame
        DEBUGF  DEBUG_NETWORK_VERBOSE, "ICMP transmitting reply\n"
        call    [ebx + NET_DEVICE.transmit]
        test    eax, eax
        jnz     @f
        DEBUGF  DEBUG_NETWORK_VERBOSE, "ICMP transmit failed\n"
        call    NET_ptr_to_num4
        inc     [ICMP_PACKETS_TX + edi]
       @@:
        ret

  .dump_:
        pusha
        mov     ecx, socket_mutex
        call    mutex_unlock
        popa

        DEBUGF  DEBUG_NETWORK_ERROR, "ICMP_input: no socket found\n"
        jmp     .dump

  .checksum_mismatch:
        DEBUGF  DEBUG_NETWORK_ERROR, "ICMP_input: checksum mismatch\n"

  .dump:
        DEBUGF  DEBUG_NETWORK_VERBOSE, "ICMP_input: dumping\n"
        call    NET_BUFF_free
        ret


if 0
;-----------------------------------------------------------------
;
; ICMP_output
;
; IN:  eax = dest ip
;      bh  = type
;      bl  = code
;      ecx = data length
;      edx = source ip
;      esi = data offset
;      edi = identifier shl 16 + sequence number
;
;-----------------------------------------------------------------
align 4
ICMP_output:

        DEBUGF  DEBUG_NETWORK_VERBOSE, "Creating ICMP Packet\n"

        push    esi edi bx
        add     ecx, sizeof.ICMP_header
        mov     di, IP_PROTO_ICMP SHL 8 + 128           ; TTL
        call    IPv4_output
        jz      .exit

        DEBUGF  DEBUG_NETWORK_VERBOSE, "full icmp packet size: %u\n", edx

        pop     word [edi + ICMP_header.Type]           ; Write both type and code bytes at once
        pop     dword [edi + ICMP_header.Identifier]    ; identifier and sequence number
        mov     [edi + ICMP_header.Checksum], 0

        push    ebx ecx edx
        mov     esi, edi
        xor     edx, edx
        call    checksum_1
        call    checksum_2
        mov     [edi + ICMP_header.Checksum], dx
        pop     edx ecx ebx esi

        sub     ecx, sizeof.ICMP_header
        add     edi, sizeof.ICMP_header
        push    cx
        shr     cx, 2
        rep movsd
        pop     cx
        and     cx, 3
        rep movsb

        sub     edi, edx                                ;;; TODO: find a better way to remember start of packet
        push    edx edi
        DEBUGF  DEBUG_NETWORK_VERBOSE, "Sending ICMP Packet\n"
        call    [ebx + NET_DEVICE.transmit]
        test    eax, eax
        jnz     @f
        call    NET_ptr_to_num4
        inc     [ICMP_PACKETS_TX + edi]
       @@:
        ret
  .exit:
        DEBUGF  DEBUG_NETWORK_ERROR, "Creating ICMP Packet failed\n"
        add     esp, 2*4 + 2
        ret
end if




;-----------------------------------------------------------------
;
; ICMP_output_raw
;
; IN:  eax = socket ptr
;      ecx = data length
;      esi = data offset
;
;-----------------------------------------------------------------
align 4
ICMP_output_raw:

        DEBUGF  DEBUG_NETWORK_VERBOSE, "Creating ICMP Packet for socket %x, data ptr=%x\n", eax, edx

        push    edx

        mov     di, IP_PROTO_ICMP SHL 8 + 128  ; TTL
        mov     edx, [eax + IP_SOCKET.LocalIP]
        mov     ebx, [eax + IP_SOCKET.device]
        mov     eax, [eax + IP_SOCKET.RemoteIP]
        call    IPv4_output
        jz      .exit

        pop     esi
        push    eax

        push    edi ecx
        DEBUGF  DEBUG_NETWORK_VERBOSE, "copying %u bytes from %x to %x\n", ecx, esi, edi
        rep movsb
        pop     ecx edi

        mov     [edi + ICMP_header.Checksum], 0

        mov     esi, edi
        xor     edx, edx
        call    checksum_1
        call    checksum_2
        mov     [edi + ICMP_header.Checksum], dx

        DEBUGF  DEBUG_NETWORK_VERBOSE, "Sending ICMP Packet\n"
        call    [ebx + NET_DEVICE.transmit]
        test    eax, eax
        jnz     @f
        call    NET_ptr_to_num4
        inc     [ICMP_PACKETS_TX + edi]
       @@:
        ret
  .exit:
        DEBUGF  DEBUG_NETWORK_ERROR, "Creating ICMP Packet failed\n"
        ret




;-----------------------------------------------------------------
;
; ICMP_API
;
; This function is called by system function 75
;
; IN:  subfunction number in bl
;      device number in bh
;      ecx, edx, .. depends on subfunction
;
; OUT:
;
;-----------------------------------------------------------------
align 4
ICMP_api:

        movzx   eax, bh
        shl     eax, 2

        test    bl, bl
        jz      .packets_tx     ; 0
        dec     bl
        jz      .packets_rx     ; 1

  .error:
        mov     eax, -1
        ret

  .packets_tx:
        mov     eax, [ICMP_PACKETS_TX + eax]
        ret

  .packets_rx:
        mov     eax, [ICMP_PACKETS_RX + eax]
        ret
