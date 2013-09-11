import qualified System.Environment            as Env
import qualified Data.Graph.Inductive          as G
import qualified Text.ParserCombinators.Parsec as P
import qualified Data.Text                     as T
import qualified Data.Text.Read                as R
import qualified System.IO                     as I
--import qualified Text.Printf                   as Pr
import qualified Data.List                     as L
import qualified Numeric                       as N
--import qualified Control.Concurrent            as C
import qualified Data.Char                     as C
import qualified Data.GraphViz                 as GV
import qualified Data.GraphViz.Commands.IO     as GV
--import qualified System.FilePath.Posix         as F
--import qualified Control.Monad                 as M
--import qualified Control.Concurrent            as C

type InstructionAddr     = Int
type InstructionOpcode   = String
type InstructionOperator = String
type InstructionOperands = String

data Instruction = Instruction {
	addr :: InstructionAddr, 
  opcode :: InstructionOpcode
} 

instance Show Instruction where 
  show instruction = "0x" ++ N.showHex (addr instruction) "" ++ "\t" ++ 
                     opcode instruction

{------------------------------------------------------------------------------
  The node's label encode the instruction opcode and the node's id encode the 
  instruction address.
------------------------------------------------------------------------------}
type InstructionNode = G.LNode InstructionOpcode
type ControlFlowGraph = G.Gr InstructionOpcode String

{------------------------------------------------------------------------------
  CFG is an open graph. 
------------------------------------------------------------------------------} 
data FullCFG = FullCFG {
  input :: InstructionNode,
  output :: InstructionNode,
  graph :: ControlFlowGraph
}

{------------------------------------------------------------------------------}
{-                              tool functions                                -}
{------------------------------------------------------------------------------}
instructionParser :: P.Parser (String, String)
instructionParser = 
  P.manyTill P.alphaNum P.tab >>= 
    (\x -> 
      (P.many P.anyChar >>= 
        \y -> return (x, y)))

{------------------------------------------------------------------------------}

getInstruction :: String -> Maybe Instruction
getInstruction inputLine = 
  case P.parse instructionParser "parsing error" inputLine of 
    Left _ -> Nothing
    Right (x, insOpcode) -> case R.hexadecimal (T.pack x) of 
      Left _ -> Nothing
      Right (insAddr, _) -> return Instruction { 
                                                  addr = insAddr, 
                                                  opcode = insOpcode 
                                               }

{------------------------------------------------------------------------------}

addInstructionToCFG :: Instruction -> FullCFG -> FullCFG 
addInstructionToCFG instruction initCFG = 
  let 
    newNode   = (addr instruction, opcode instruction)
    initGraph = graph initCFG 
    initNodes = G.nodes initGraph
  in 
    let 
      (newInput, newGraph) = 
        if G.isEmpty initGraph 
          then (newNode, G.insNode newNode initGraph)
          else (input initCFG, G.insEdge (fst (output initCFG), fst newNode, 
                                         "0x" ++ N.showHex (fst newNode) "") 
                                         (if fst newNode `L.elem` initNodes 
                                            then initGraph
                                            else G.insNode newNode initGraph))
    in 
      FullCFG { 
                input  = newInput, 
                output = newNode, 
                graph  = newGraph 
              }

{------------------------------------------------------------------------------}

getAndAddInstructionToCFG :: String -> FullCFG -> FullCFG 
getAndAddInstructionToCFG inputLine initCFG = 
  case getInstruction inputLine of 
    Nothing  -> initCFG
    Just ins -> addInstructionToCFG ins initCFG

{------------------------------------------------------------------------------}

instructionOpcodeParser :: P.Parser (InstructionOperator, InstructionOperands)
instructionOpcodeParser = P.manyTill P.letter P.space >>= 
  (\x -> (P.many P.anyChar >>= (\y -> return (x, y))))

{------------------------------------------------------------------------------}

getInstructionOperator :: InstructionOpcode -> Maybe InstructionOperator
getInstructionOperator instructionOpcode = 
  case P.parse instructionOpcodeParser "parsing error" instructionOpcode of 
    Left _              -> Nothing
    Right (operator, _) -> return operator

{------------------------------------------------------------------------------}

getInstructionOperands :: InstructionOpcode -> Maybe InstructionOperands 
getInstructionOperands instructionOpcode = 
  case P.parse instructionOpcodeParser "parsing error" instructionOpcode of 
    Left _              -> Nothing
    Right (_, operands) -> return operands

{------------------------------------------------------------------------------}

isUnconditionalBranch :: InstructionOpcode -> Bool
isUnconditionalBranch instructionOpcode = 
  case getInstructionOperator instructionOpcode of 
    Nothing -> False
    Just operator -> operator `elem` ["jmp", "call", "ret"] 

{------------------------------------------------------------------------------}

isConditionalBranch :: InstructionOpcode -> Bool
isConditionalBranch instructionOpcode = 
  case getInstructionOperator instructionOpcode of 
    Nothing -> False 
    Just operator -> 
      operator `elem` ["ja", "jae", "jb", "jbe", "jc", "jcxz", "je"] || 
      operator `elem` ["jg", "jge", "jl", "jle", "jna", "jnae", "jnb", "jnbe"] || 
      operator `elem` ["jnc", "jne", "jng", "jnge", "jnl", "jnle", "jno", "jnp"] || 
      operator `elem` ["jns", "jnz", "jo", "jp", "jpe", "jpo", "js", "jz"] 

{------------------------------------------------------------------------------}

removeDuplicateEdges :: FullCFG -> FullCFG 
removeDuplicateEdges initCFG = 
  let 
    initGraph   = graph initCFG
    allLabEdges = G.labEdges initGraph
    nubbedEdges = L.nubBy (\(x1,x2,_) (y1,y2,_) -> (x1 == y1) && (x2 == y2))    -- remove edges of the same head and tail
                          allLabEdges
    newGraph    = G.delEdges (G.edges initGraph) initGraph
  in 
    FullCFG { 
              input = input initCFG, 
              output = output initCFG, 
              graph = G.insEdges nubbedEdges newGraph 
            }

{------------------------------------------------------------------------------}

isBranchNode :: ControlFlowGraph -> InstructionAddr -> Bool
isBranchNode initGraph initAddr = 
  case G.lab initGraph initAddr of 
    Nothing    -> False 
    Just label -> isConditionalBranch label || isUnconditionalBranch label

{------------------------------------------------------------------------------}

isSingleInput :: FullCFG -> InstructionNode -> Bool                             -- a single input node is a node with an unique incoming edge
isSingleInput initCFG initInsNode = 
  L.length (G.pre (graph initCFG) (fst initInsNode)) == 1

{------------------------------------------------------------------------------}

isSingleOutput :: FullCFG -> InstructionNode -> Bool                            -- a single output node is a node with an unique outcoming edge
isSingleOutput initCFG initInsNode = 
  L.length (G.suc (graph initCFG) (fst initInsNode)) == 1  

{------------------------------------------------------------------------------}

isExplicitUnconditionalBranch :: InstructionNode -> Bool
isExplicitUnconditionalBranch initInsNode = 
  let 
    insOpcode = snd initInsNode
  in 
    case getInstructionOperator insOpcode of 
      Nothing       -> False
      Just operator -> case getInstructionOperands insOpcode of 
        Nothing       -> False
        Just operands -> operator `L.elem` ["jmp"{--,"call"--}] && 
                         isDirectAddress operands
        where 
          isDirectAddress x = case R.hexadecimal (T.pack x) of 
            Left _                  -> False
            Right (_, remainingTxt) -> remainingTxt == T.empty

{------------------------------------------------------------------------------}

isBranch :: FullCFG -> InstructionNode -> Bool                                  -- a branch node is a jump node
isBranch initCFG initInsNode = 
  isBranchNode (graph initCFG) (fst initInsNode)

{------------------------------------------------------------------------------}

isConnected :: FullCFG -> InstructionNode -> InstructionNode -> Bool
isConnected initCFG insNode1 insNode2 = 
  let 
    node1     = fst insNode1
    node2     = fst insNode2
    initGraph = graph initCFG
  in 
    (node2 `elem` G.pre initGraph node1) || 
    (node2 `elem` G.suc initGraph node1)

{------------------------------------------------------------------------------}

groupNodes :: FullCFG -> [InstructionNode] -> [[InstructionNode]]               -- group the sequential instructions into a group,
groupNodes initCFG initInsNodes =                                               -- each group is represented by a list [InstructionNode],
  case initInsNodes of                                                          -- so we have a list of group.
    []  -> [[]]
    [insNode] -> [[insNode]]
    (insNode:otherNodes) 
      | isBranch initCFG insNode && 
        not (isExplicitUnconditionalBranch insNode)           -> 
          [insNode]:groupNodes initCFG otherNodes

      | not (isConnected initCFG insNode (L.head otherNodes)) -> 
          [insNode]:groupNodes initCFG otherNodes

      | not (isSingleOutput initCFG insNode)                  -> 
          [insNode]:groupNodes initCFG otherNodes

      | not (isSingleInput initCFG (L.head otherNodes))       -> 
          [insNode]:groupNodes initCFG otherNodes

      | otherwise                                             -> 
          (insNode:subOtherNodes):otherGroups
      where 
        (subOtherNodes:otherGroups) = groupNodes initCFG otherNodes  

{------------------------------------------------------------------------------}
      
compressNodes :: [InstructionNode] -> FullCFG -> FullCFG                        -- compress a sequence (without jump between) 
compressNodes initInsNodes initCFG =                                            -- of instructions into a single group (as a new node).
  if L.length initInsNodes == 1
    then 
      initCFG
    else 
      let 
        initGraph       = graph initCFG
        initNodes       = L.map fst initInsNodes
        initLabels      = L.map snd initInsNodes 
        newLabel        = L.foldl (\x y -> x ++ "\n" ++ y) (L.head initLabels)  -- the new node has its index as the index of the first component
                                  (L.tail initLabels)                           -- and its label as the new label. 
        newLNode        = ((fst . L.head) initInsNodes, newLabel)               

        redirectedEdges = [(x, fst newLNode, l)                                 -- its outcoming and incoming edges is calculated by 
                              | (x, l) <- G.lpre initGraph (L.head initNodes),  -- redirecting component's edges.
                                          x /= L.last initNodes] ++ 
                            [(fst newLNode, y, l) 
                              | (y, l) <- G.lsuc initGraph (L.last initNodes), 
                                          y /= L.head initNodes]
        
        newEdges        = case L.find (\(x, y, _) -> x == L.last initNodes &&   -- should be careful that there is a case where the new node 
                                                     y == L.head initNodes)     -- has a loopback edge.
                                      (G.labEdges initGraph) of
            Nothing        -> redirectedEdges
            Just (_, _, l) -> (fst newLNode, fst newLNode, l):redirectedEdges   -- loopback edge.
        
        newInputNode    = if fst newLNode == fst (input initCFG) 
                              then newLNode else input initCFG
      in
        let
          trimmedGraph = G.delNodes initNodes initGraph
          updatedGraph = G.insNode newLNode trimmedGraph
          finalGraph   = G.insEdges newEdges updatedGraph 
        in 
          FullCFG {                                                             -- here is the new node.
                    input  = newInputNode, 
                    output = output initCFG, 
                    graph  = finalGraph
                  }

{------------------------------------------------------------------------------}

fastCompressCFG :: FullCFG -> FullCFG
fastCompressCFG initCFG = 
  let 
    initGraph = graph initCFG
    initNodes = G.dfs [(fst . input) initCFG] initGraph 
  in 
    case mapM (G.lab initGraph) initNodes of
      Nothing         -> initCFG
      Just initLabels -> L.foldr compressNodes 
                                 initCFG $ groupNodes initCFG 
                                            (L.zip initNodes initLabels)

{------------------------------------------------------------------------------}

printDotCFG :: String -> FullCFG -> IO () 
printDotCFG file cfg = I.writeFile file (G.graphviz (graph cfg) 
                                         "ControlFlowOpenGraph" (8.3,11.7) (0,0) 
                                         G.Portrait)

{------------------------------------------------------------------------------}

printDotGraph :: String -> FullCFG -> IO ()
printDotGraph fileName initCFG = do 
  let 
    initGraph    = graph initCFG
    inputNode    = input initCFG
    
    setNodeParam = \x -> 
      if fst x == fst inputNode 
        then [GV.color GV.Orange, GV.style GV.filled, GV.toLabel (snd x)] 
        else [GV.color GV.Black, GV.toLabel (snd x)]
    
    setEdgeLabel = \(_,_,l) -> [GV.toLabel l]
    gvParam      = GV.nonClusteredParams { 
                                           GV.fmtNode = setNodeParam, 
                                           GV.fmtEdge = setEdgeLabel 
                                         }
    gvDot        = GV.graphToDot gvParam initGraph 
  
  GV.writeDotFile fileName gvDot 



{------------------------------------------------------------------------------}
{-                            test functions                                  -}
{------------------------------------------------------------------------------}



{------------------------------------------------------------------------------}
{-                             main function                                  -}
{------------------------------------------------------------------------------}
printCFG :: String -> IO ()
printCFG fileName = do 
  fileContent <- I.readFile fileName 
  let 
    tracedLines = L.reverse $ lines fileContent
    initCFG     = FullCFG { 
                            input = (0,""), 
                            output = (0, ""), 
                            graph = G.empty 
                          }
    rawCFG      = L.foldr getAndAddInstructionToCFG initCFG tracedLines
    trimCFG     = removeDuplicateEdges rawCFG 
    finalCFG    = fastCompressCFG trimCFG

    --procID      = L.takeWhile C.isDigit fileName

  putStrLn (show (L.length tracedLines) ++ " instructions captured.")

  printDotGraph "raw_trace.dot" rawCFG >> 
    putStrLn (show ((L.length . G.nodes . graph) rawCFG) ++ 
              " nodes in raw graph.")
    
  printDotGraph "compressed_trace.dot" finalCFG >> 
    putStrLn (show ((L.length . G.nodes . graph) finalCFG) ++ 
                 " nodes in compressed graph.")

{------------------------------------------------------------------------------}

main :: IO ()
main = do 
  args <- Env.getArgs 
  case args of 
    (inputTraceFile : _) -> printCFG inputTraceFile
    _ -> putStrLn "Please run as: this_file input_trace_file"

