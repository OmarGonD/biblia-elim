"""
Ensambla la corriente de sucesos en (libro, capítulo, versículo).

Contar capítulos en secuencia no vale: en cuanto el OCR se come un
encabezado, todo lo que viene detrás queda corrido y el libro entero se
descoloca. Así que se segmenta primero en capítulos candidatos -- por
encabezado impreso o por reinicio de la numeración -- y esa secuencia se
alinea contra el canon con programación dinámica, permitiendo que un
capítulo real se haya partido en dos, que sobre un candidato espurio o
que falte uno entero. Un fallo suelto deja de arrastrar a los siguientes.
"""

def segmenta(sucesos):
    """Parte la corriente en capítulos candidatos."""
    caps, cur, ult = [], None, 0
    def nuevo():
        nonlocal cur, ult
        cur = {"nums": [], "sucesos": [], "cands": [], "inicio": False}
        caps.append(cur); ult = 0
    for s in sucesos:
        if s[0] == "libro":
            nuevo()
            cur["inicio"] = True
        elif s[0] == "cap":
            if cur is None or cur["sucesos"] or not cur["inicio"]:
                marca = cur["inicio"] if cur is not None and not cur["sucesos"] else False
                nuevo()
                cur["inicio"] = marca
        elif s[0] == "vers":
            n = s[1]
            if cur is None or (n <= 2 and ult >= 5):
                nuevo()
            cur["nums"].append(n)
            cur["sucesos"].append(s)
            if s[3]:
                cur["cands"].append(s[3])
            if n <= 200:
                ult = max(ult, n)
        else:
            if cur is not None:
                cur["sucesos"].append(s)
    return [c for c in caps if c["sucesos"]]

def puntua(cap, osis, n_canon, n_cap=0):
    """
    Cómo de bien encaja un capítulo candidato en un capítulo de n_canon
    versos del libro osis: por los números leídos y por lo que dice la
    cabecera corrida de las páginas de donde salió.
    """
    nums = cap["nums"]
    if not nums:
        return -1.0
    dentro = sum(1 for v in nums if 1 <= v <= n_canon)
    fuera = len(nums) - dentro
    plaus = [v for v in nums if 1 <= v <= n_canon]
    cierre = 0.0
    if plaus:
        cierre = 3.0 * (1.0 - min(1.0, abs(max(plaus) - n_canon) / max(1, n_canon)))
    # La cabecera corrida dice en qué libro va la página. Es señal impresa
    # y directa, así que pesa más que el encaje de los números: sin ella,
    # el alineador se comía el capítulo 1 de ocho libros y corría la
    # frontera, dejando Malaquías 4 dentro de I Macabeos.
    # Una portada delante solo casa con el capítulo 1 de un libro.
    if cap.get("inicio"):
        if n_cap != 1:
            return -50.0
    voto = 0.0
    if cap["cands"]:
        favor = sum(1 for c in cap["cands"] if osis in c)
        voto = 10.0 * (2.0 * favor / len(cap["cands"]) - 1.0)
    return dentro * 1.0 - fuera * 2.0 + cierre + voto

def alinea_capitulos(obs, canon_caps):
    """
    obs:        [capítulo candidato]
    canon_caps: [(osis, cap, n_versos)]
    Devuelve [(osis, cap, [índices de obs])]
    """
    n, m = len(obs), len(canon_caps)
    NEG = float("-inf")
    D = [[NEG] * (m + 1) for _ in range(n + 1)]
    P = [[None] * (m + 1) for _ in range(n + 1)]
    D[0][0] = 0.0
    for i in range(n + 1):
        for j in range(m + 1):
            base = D[i][j]
            if base == NEG:
                continue
            if i < n and j < m:
                s = base + puntua(obs[i], canon_caps[j][0], canon_caps[j][2], canon_caps[j][1])
                if s > D[i + 1][j + 1]:
                    D[i + 1][j + 1] = s; P[i + 1][j + 1] = (i, j, "1")
            if i + 1 < n and j < m:      # dos candidatos = un capítulo real
                fus = {"nums": obs[i]["nums"] + obs[i + 1]["nums"],
                       "cands": obs[i]["cands"] + obs[i + 1]["cands"],
                       "inicio": obs[i].get("inicio", False)}
                s = base + puntua(fus, canon_caps[j][0], canon_caps[j][2],
                                  canon_caps[j][1]) - 1.0
                if s > D[i + 2][j + 1]:
                    D[i + 2][j + 1] = s; P[i + 2][j + 1] = (i, j, "2")
            if i < n:                    # candidato espurio, se descarta
                s = base - 3.0
                if s > D[i + 1][j]:
                    D[i + 1][j] = s; P[i + 1][j] = (i, j, "x")
            if j < m:                    # capítulo que el OCR no vio
                s = base - 3.0
                if s > D[i][j + 1]:
                    D[i][j + 1] = s; P[i][j + 1] = (i, j, "0")
    # reconstrucción
    out, i, j = [], n, m
    while (i, j) != (0, 0):
        p = P[i][j]
        if p is None:
            break
        pi, pj, mv = p
        if mv == "1":
            out.append((canon_caps[pj][0], canon_caps[pj][1], [pi]))
        elif mv == "2":
            out.append((canon_caps[pj][0], canon_caps[pj][1], [pi, pi + 1]))
        elif mv == "0":
            out.append((canon_caps[pj][0], canon_caps[pj][1], []))
        i, j = pi, pj
    out.reverse()
    return out

def ensambla(sucesos, libros):
    """Devuelve ({(osis,cap,ver): [trozos]}, [avisos])."""
    obs = segmenta(sucesos)
    canon_caps = [(L["osis"], c, n)
                  for L in libros
                  for c, n in enumerate(L["versos"], start=1)]
    plan = alinea_capitulos(obs, canon_caps)

    vers, avisos = {}, []
    for osis, cap, idxs in plan:
        if not idxs:
            avisos.append(f"{osis} {cap}: capítulo sin texto")
            continue
        tope = dict(((o, c), n) for o, c, n in canon_caps)[(osis, cap)]
        ver, actual = 0, None
        for k in idxs:
            for s in obs[k]["sucesos"]:
                if s[0] == "vers":
                    n = s[1]
                    if s[4] and 1 <= n <= tope:
                        # Marca de principio de renglón: el número está
                        # impreso y es más de fiar que el contador. Si va
                        # hacia atrás es que antes se coló un versículo
                        # espurio, así que se hace caso al papel y la
                        # cuenta se recoloca sola. Llevar el contador por
                        # delante corría el capítulo entero: 3 Reyes 22
                        # acababa con cada versículo un puesto más abajo.
                        ver = n
                    elif 1 <= n <= tope and n > ver:
                        ver = n
                    else:
                        # marca dudosa -- un corte dentro del renglón que
                        # no cuadra --: pegar al versículo en curso. Antes
                        # se inventaba el número y eso corría el capítulo
                        # entero un puesto: 2 Par 6:39 acababa con el 38.
                        if actual:
                            vers[actual].append(s[2])
                        continue
                    actual = (osis, cap, ver)
                    vers.setdefault(actual, []).append(s[2])
                elif actual:
                    vers[actual].append(s[1])
    return vers, avisos
